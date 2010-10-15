/* 
 * wps-object 0.1 (c) 2010 Yuri Dario
 *
 * Create and delete WPS objects.
 * Register and deregister WPS classes.
 *
 * Keeps track of installed/removed objects into %UNIXROOT%\cache\rpm_wps\{name}
 *
 */

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

parse arg name " " action "$" classname "$" title "$" location "$" setup "$" option

/*
say "name:"name
say "action:"action
say "classname:"classname
say "title:"title
say "location:"location
say "setup:"setup
say "option:"option
*/

/* create cache dir */
UNIXROOT = VALUE('UNIXROOT',,'OS2ENVIRONMENT')
cache_dir = UNIXROOT || "\var\cache\rpm_wps"
rc = sysMkDir( cache_dir)
cache_file = cache_dir || "\" || name

/* cleanup and unescape parameters */
action = convert( action)
classname = convert( classname)
title = convert( title)
location = convert( location)
setup = convert( setup)
option = convert( option)

select
when action = "/create" then rc = create( cache_file, classname, title, location, setup, option)
when action = "/delete" then rc = delete( cache_file, classname)
when action = "/deleteall" then rc = deleteall( cache_file)
when action = "/register" then
  rc = SysRegisterObjectClass( classname, title)
when action = "/deregister" then
  rc = SysDeregisterObjectClass( classname)
otherwise
  say "unknown opt:" action
end

if rc = 0 then say 'Operation failed!'

exit 0


create: procedure
  parse arg cache, classname, title, location, setup, option

  if left(location,1) \= '<' then
    location = '<' || location || '>'

  if option = '' then option = 'update'

  rc = SysCreateObject( classname, title, location, setup, option)

  call lineout cache, classname || x2c(7) || title || x2c(7) || location || x2c(7) || setup || x2c(7) || option
  call lineout cache
  return rc


delete: procedure
  parse arg cache, objid

  if left(objid,1) \= '<' then
    objid = '<' || objid || '>'
  rc = SysDestroyObject( objid)

  /* remove id from cache file if specified */
  if cache = '' then
    return rc

  /* move to temp file */
  cache_bak = cache || ".bak"
  rc2 = SysFileDelete( cache_bak)
  '@move 'cache ' ' cache_bak ' > \dev\nul 2> \dev\nul'
  /* scan list */
  do while( lines( cache_bak))
    setup = linein( cache_bak)
    if pos( objid, setup) = 0 then
      call lineout cache, setup
  end
  /* close file */
  call lineout cache

  /* delete temp file */
  rc2 = SysFileDelete( cache_bak)

  return rc


deleteall: procedure
  parse arg cache

  i = 0

  /* scan list */
  do while( lines( cache))
    setup = linein( cache) || ';'
    o = pos( 'OBJECTID=', setup)
    if o > 0 then do
      t = pos( ';', setup, o)
      objid = substr( setup, o+9, t - o - 9)
      list.i = objid
      i = i + 1
    end
  end
  /* close file */
  call lineout cache

  /* delete in reverse order */
  do j = 1 to i
    o = i - j
    rc = delete( '', list.o)
  end

  /* delete temp file */
  rc2 = SysFileDelete( cache)

  return 1

convert: procedure
  parse arg _str

  _str = strip( _str)
  if left(_str,1) = '"' & right(_str,1) = '"' then
     _str = substr(_str,2,length(_str)-2)

  c = pos('#lt#',_str)
  if c>0 then _str = left(_str,c-1) || '<' || substr(_str,c+4)

  c = pos('#gt#',_str)
  if c>0 then _str = left(_str,c-1) || '>' || substr(_str,c+4)

  c = pos('#35#',_str)
  if c>0 then _str = left(_str,c-1) || '#' || substr(_str,c+4)

  c = pos('#36#',_str)
  if c>0 then _str = left(_str,c-1) || '$' || substr(_str,c+4)

  return _str
