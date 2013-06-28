=== Introduction === 
incpath is a PHP extension to "resolve" a portion of include_path set in PHP's configuration INI files.

There are 3 configuration values:
* search_replace_pattern: This is the path string to look for in include_path. incpath applies a simple string comparison to determine a match: no regexes or wildcards allowed. If no match is found for the pattern, incpath will do nothing. If a match is found, incpath will replace the entire matched string with the "resolved" path depending on the SAPI configuration.
* realpath_sapi_list: Comma-separated list of SAPIs where incpath will realpath(3) the search_replace_pattern and in-place replace it in include_path (this is only done if a match was found).
* docroot_sapi_list: Comma-separated list of SAPIs where incpath will in-place replace search_replace_pattern with $_SERVER['DOCUMENT_ROOT'] in include_path (this is only done if a match was found).

incpath is intended to be used for atomic changes to a large, deployed PHP application in conjunction with mod_realdoc. Usually such an application has 2 deploy locations: one active and the other inactive. A symlink to the active one is referenced in the DOCUMENT_ROOT in the web server's configuration, and in PHP's include_path. 

By hooking in before any actual PHP code executes, incpath "resolves" the symlink exactly once, and all subsequent users of include_path (like require/require_once/include/include_once) never have to resolve it again, thereby ensuring the entire request references code in only one location.

=== Installation ===
* phpize
* ./configure
* make
* make install
