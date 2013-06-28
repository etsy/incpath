/*
 * Copyright (c) 2013 Etsy
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_incpath.h"
#include "main/php_reentrancy.h"
#include "ext/standard/php_string.h"
#include "main/SAPI.h"

ZEND_DECLARE_MODULE_GLOBALS(incpath)

/* True global resources - no need for thread safety here */
static int le_incpath;

/* {{{ incpath_module_entry
 */
zend_module_entry incpath_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"incpath",
	NULL,
	PHP_MINIT(incpath),
	PHP_MSHUTDOWN(incpath),
	PHP_RINIT(incpath),
	PHP_RSHUTDOWN(incpath),
	PHP_MINFO(incpath),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* version number */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_INCPATH
ZEND_GET_MODULE(incpath)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("incpath.search_replace_pattern", NULL, PHP_INI_SYSTEM, OnUpdateString, search_replace_pattern, zend_incpath_globals, incpath_globals)
    STD_PHP_INI_ENTRY("incpath.realpath_sapi_list", NULL, PHP_INI_SYSTEM, OnUpdateString, realpath_sapi_list, zend_incpath_globals, incpath_globals)
    STD_PHP_INI_ENTRY("incpath.docroot_sapi_list", NULL, PHP_INI_SYSTEM, OnUpdateString, docroot_sapi_list, zend_incpath_globals, incpath_globals)
PHP_INI_END()
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 *  */
PHP_MINIT_FUNCTION(incpath)
{
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 *  */
PHP_MSHUTDOWN_FUNCTION(incpath)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ php_incpath_init_globals
 */
static void php_incpath_init_globals(zend_incpath_globals *incpath_globals)
{
	incpath_globals->search_replace_pattern = NULL;
	incpath_globals->docroot_sapi_list = NULL;
	incpath_globals->realpath_sapi_list = NULL;
}
/* }}} */

#define ACTION_NOTHING 0
#define ACTION_REPLACE_WITH_DOCROOT 1
#define ACTION_REPLACE_WITH_REALPATH 2

/*
 * If sapi_list contains the current SAPI name, returns true.
 * Else returns false.
 */
static int evaluate_sapi(char* sapi_list) {
	char *sapi_list_copy;
	char *current_sapi;
	char *strtok_last;

	sapi_list_copy = estrdup(sapi_list);
	current_sapi = php_strtok_r(sapi_list_copy, ",", &strtok_last);
	while (current_sapi) {
		char *trimmed_sapi = php_trim(current_sapi, (int)(strlen(current_sapi)), NULL, 0, NULL, 3 TSRMLS_CC);
		if (!strcmp(sapi_module.name, trimmed_sapi)) {
			efree(trimmed_sapi);
			efree(sapi_list_copy);
			return 1;
		}

		efree(trimmed_sapi);
		current_sapi = php_strtok_r(NULL, ",", &strtok_last);
	}
	efree(sapi_list_copy);
	return 0;
}

static void realloc_include_path_string(char **str_ptr, int *str_len, int expected_len) {
	if (expected_len > *str_len) {
		*str_len = expected_len;
		*str_ptr = erealloc(*str_ptr, *str_len);
	}

}

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(incpath)
{
	char *original_include_path;
	char *pattern_substr_start;
	int action = ACTION_NOTHING;

	if (INCPATH_G(docroot_sapi_list) && evaluate_sapi(INCPATH_G(docroot_sapi_list)))
		action = ACTION_REPLACE_WITH_DOCROOT;
	else if (INCPATH_G(realpath_sapi_list) && evaluate_sapi(INCPATH_G(realpath_sapi_list)))
		action = ACTION_REPLACE_WITH_REALPATH;
	else
		return SUCCESS;

	original_include_path = zend_ini_string("include_path", sizeof("include_path"), 0);
	if (INCPATH_G(search_replace_pattern) && ((pattern_substr_start = strstr(original_include_path, INCPATH_G(search_replace_pattern))) != NULL)) {
		int new_include_path_offset = 0;
		int original_include_path_len = strlen(original_include_path);
		int new_include_path_len = (2 * original_include_path_len) + 1; // Should help avoid multiple realloc'ing
		char *new_include_path = ecalloc(new_include_path_len, sizeof(char));
		int remaining_length = 0;
		int search_replace_pattern_len = strlen(INCPATH_G(search_replace_pattern));

		if (pattern_substr_start != original_include_path) {
			new_include_path_offset = pattern_substr_start - original_include_path;
			strncpy(new_include_path, original_include_path, new_include_path_offset);
		}

		if (action == ACTION_REPLACE_WITH_DOCROOT) {
			zval **server_doc_root;
			char *doc_root_ptr;
			int doc_root_ptr_len;
			zval **array;

			// This will load it up, if not already loaded.
			zend_is_auto_global("_SERVER", strlen("_SERVER"));
			if ((zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &array) == SUCCESS) &&
			    (Z_TYPE_PP(array) == IS_ARRAY) &&
			    (zend_hash_find(Z_ARRVAL_PP(array), "DOCUMENT_ROOT", sizeof("DOCUMENT_ROOT"), (void **) &server_doc_root) == SUCCESS) &&
			    (Z_TYPE_PP(server_doc_root) == IS_STRING) &&
			    (doc_root_ptr_len = strlen(doc_root_ptr = Z_STRVAL_PP(server_doc_root)))) {
				realloc_include_path_string(&new_include_path, &new_include_path_len, (new_include_path_offset + doc_root_ptr_len + 1));
				strncpy(new_include_path + new_include_path_offset, doc_root_ptr, doc_root_ptr_len);
				new_include_path_offset += doc_root_ptr_len;
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "incpath called for docroot replacement, but $_SERVER['DOCUMENT_ROOT'] does not exist or is an empty string");
				goto err;
			}

		} else { // if (action == ACTION_REPLACE_WITH_REALPATH)
			char *current_token;
			char *strtok_last;
			char *pattern_copy;
			char *realpath_str;
			int append_delim = 0;
			pattern_copy = estrdup(INCPATH_G(search_replace_pattern));
			current_token = php_strtok_r(pattern_copy, ":", &strtok_last);
			while (current_token) {
				if (append_delim) {
					new_include_path[new_include_path_offset] = ':';
					++new_include_path_offset;
					append_delim = 0;
				}
				char *realpath_str = realpath(current_token, NULL);
				if (realpath_str == NULL) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING,
							 "realpath() in incpath failed for %s with errno %d", current_token, errno);
					efree(pattern_copy);
					goto err;
				} else {
					int realpath_len = strlen(realpath_str);
					realloc_include_path_string(&new_include_path, &new_include_path_len, (new_include_path_offset + realpath_len + 2));
					strncpy(new_include_path + new_include_path_offset, realpath_str, realpath_len);
					new_include_path_offset += realpath_len;
					append_delim = 1;
					free(realpath_str);
				}

				current_token = php_strtok_r(NULL, ":", &strtok_last);
			}

			efree(pattern_copy);
		}

		// Replace remaining string into new_include_path
		remaining_length = strlen(pattern_substr_start + search_replace_pattern_len);
		realloc_include_path_string(&new_include_path, &new_include_path_len, (new_include_path_offset + remaining_length + 1));

		if (remaining_length) {
			strncpy(new_include_path + new_include_path_offset, pattern_substr_start + search_replace_pattern_len, remaining_length);
			new_include_path_offset += remaining_length;
		}
		new_include_path[new_include_path_offset] = '\0';
		if (zend_alter_ini_entry_ex("include_path", sizeof("include_path"), new_include_path, new_include_path_offset, PHP_INI_USER, PHP_INI_STAGE_RUNTIME, 0 TSRMLS_CC) == FAILURE) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
					 "Error setting include_path INI variable");
			efree(new_include_path);
			goto err;
		}

		efree(new_include_path);
	}


err: // Not exiting with FAILED because then Apache/CLI will fail to start at all!
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(incpath)
{
	zend_restore_ini_entry("include_path", sizeof("include_path"), PHP_INI_STAGE_RUNTIME);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(incpath)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "incpath support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
