#!/bin/sh

# TODO fix this script to accept $2 instead of stdin
# re-read file to stdin...
exec 0<$2

[ $# -ge 1 ] || {
    cat > /dev/null
    exit 0
}

PYVER=`python -c "import sys; v=sys.version_info[:2]; print '%d.%d'%v"`
case $1 in
-P|--provides)
    shift
    grep "/usr/bin/python\*\$" >& /dev/null && echo "python(abi) = ${PYVER}"
    exit 0
    ;;
-R|--requires)
    shift
    grep "/usr/lib[^/]*/python${PYVER}/" >& /dev/null && echo "python(abi) = ${PYVER}"
    exit 0
    ;;
esac

exit 0
