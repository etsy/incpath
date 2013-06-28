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
 * 
 */

#ifndef PHP_INCPATH_H
#define PHP_INCPATH_H

extern zend_module_entry incpath_module_entry;
#define phpext_incpath_ptr &incpath_module_entry

#ifdef PHP_WIN32
#	define PHP_INCPATH_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_INCPATH_API __attribute__ ((visibility("default")))
#else
#	define PHP_INCPATH_API
#endif

ZEND_BEGIN_MODULE_GLOBALS(incpath)
	char *search_replace_pattern;
	char *docroot_sapi_list;
	char *realpath_sapi_list;
ZEND_END_MODULE_GLOBALS(incpath)

#ifdef ZTS
#include "TSRM.h"
#define INCPATH_G(v) TSRMG(incpath_globals_id, zend_incpath_globals *, v)
#else
#define INCPATH_G(v) (incpath_globals.v)
#endif

PHP_MINIT_FUNCTION(incpath);
PHP_MSHUTDOWN_FUNCTION(incpath);
PHP_RINIT_FUNCTION(incpath);
PHP_RSHUTDOWN_FUNCTION(incpath);
PHP_MINFO_FUNCTION(incpath);

#endif	/* PHP_INCPATH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
