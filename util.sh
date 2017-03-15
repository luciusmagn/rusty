if ! [ $# -lt 2 ]
then
    echo usage: $0 'doc|html|installdoc|rmdoc'
    exit -1
fi

case "$1" in
    doc)
        nroff -man rusty.1
        ;;
    html)
        groff -Thtml -man rusty.1 > README.md
        sed -i '1,20d' README.md
        sed -i '/<\/body>/d' README.md
        sed -i '/<\/html>/d' README.md
        ;;
    installdoc)
        cp rusty.1 /usr/man/man1
        echo updating mandb
        mandb -q
        ;;
    rmdoc)
        rm -f /usr/man/man1/rusty.1
        echo updating mandb
        mandb -q
        ;;
esac
