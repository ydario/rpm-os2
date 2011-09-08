/* REXX */

/*
 * Takes a list of WarpIn package IDs on the standard input and returns
 * 1 if any of these packages is installed, otherwise returns 0.
 *
 * Each line in the input stream is a package ID in the format
 * "Vendor\Application\Package". Empty lines are ignored.
 *
 * If there is a conflict, prints a warning message with the installed package
 * and its version to the standard output. Prints nothing otherwise.
 *
 * Author: Dmitriy Kuminov
 *
 * Version: 1.1 - 2011-09-08
 *   - Skip incomplete IDs (they will not match anything).
 *   - If the argment is given, treat as ID, print version (if any) and exit.
 * Version: 1.0 - 2011-09-06
 *   - Initial.
 */

trace off
numeric digits 12
'@echo off'

/*------------------------------------------------------------------------------
 globals
------------------------------------------------------------------------------*/

/* all globals to be exposed in procedures */
Globals = 'G. Opt. Static.'

/*------------------------------------------------------------------------------
 startup + main + termination
------------------------------------------------------------------------------*/

/* init system REXX library */
if (RxFuncQuery('SysLoadFuncs')) then do
    call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
    call SysLoadFuncs
end

parse arg G.Args

return Main()

/*------------------------------------------------------------------------------
 functions
------------------------------------------------------------------------------*/

/**
 * Just do the job.
 *
 * @param aArgs Comand line arguments.
 * @return      0 on success, error code on failure.
 */
Main: procedure expose (Globals)

    if (G.Args \== '') then do
        ver = GetPkgVersion(G.Args)
        if (ver \== '') then do
            say ver
            exit 1
        end
        exit 0
    end

    i = 0
    do forever
        id = linein()
        if (strip(id) == '') then do
            if (stream('STDIN', 'S') \== 'READY') then leave
            iterate
        end
        i = i + 1
        packages.i = id
    end
    packages.0 = i

    ver = ''
    do i = 1 to packages.0
        ver = GetPkgVersion(packages.i)
        if (ver \== '') then leave
    end
    if (ver == '') then exit 0

    say; say 'ERROR:'; say
    say 'The following WPI package installed on your system conflicts with the'
    say 'RPM package being installed:'; say
    say '  'packages.i' (version 'ver')'; say
    say 'You cannot have both the WPI and the RPM package installed at the same'
    say 'time. Please de-install the specified WPI package using the WarpIn utility'
    say 'and try again.'; say
    exit 1

/**
 * Returns the version for the given package ID or '' if this package
 * is not installed.
 *
 * @param aPkgId    Package ID (vendor\application\package).
 * @return          Package version or ''.
 */
GetPkgVersion: procedure
    parse arg aPkgId
    parse var aPkgId v1'\'a1'\'p1
    if (v1 == '' | a1 == '' | p1 == '') then return ''
    WarpInDir = strip(SysIni('USER', 'WarpIN', 'Path'), 'T', '0'x)
    if (WarpInDir \== '') then do
        rc = SysFileTree(WarpInDir'\DATBAS_?.INI', 'inis', 'FO')
        if (rc == 0) then do
            do i = 1 to inis.0
                rc = SysIni(inis.i, 'ALL:', 'apps')
                if (rc == '') then do
                    do j = 1 to apps.0
                        apps.j = strip(apps.j, 'T', '0'x)
                        parse var apps.j v2'\'a2'\'p2'\'ver
                        if (v1 == v2 & a1 == a2 & p1 == p2) then do
                            /* found the app */
                            ver = translate(ver, '.', '\')
                            return ver
                        end
                    end
                end
                else do
                    say; say 'ERROR:'; say
                    say 'Failed to access the WarpIn database file:'; say
                    say '  'inis.i; say
                    say 'Please close the WarpIn application or, if it is not running, make sure'
                    say 'that this file is not locked by another process, and try again.'; say
                    exit 5
                end
            end
        end
    end
    return ''
