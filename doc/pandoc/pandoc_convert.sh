#! /bin/bash

if test "$#" -ne 1 ; then
    echo 1>&2 "Usage pandoc_convert.sh <latex, pdf>"
    exit 1
fi

# Search for the ngpt directory
array=()
while IFS=  read -r -d $'\0'; do
        array+=("$REPLY")
done < <(find ~/ -type d -name ngpt -print0)

if test "${#array[@]}" -eq 0 ; then
    echo 1>&2 "ERROR. Could not find the /ngpt dir."
    exit 1
fi

# Set the NGPT_DIR
if test "${#array[@]}" -gt 1 ; then
    echo "More than one /ngpt dirs found. Choose the correct (i.e. the root):"
    idx=1
    for i in "${array[@]}" ; do
        echo "$idx $i"
        let idx=idx+=1
    done
    echo "Which do you want? [1-${#array[@]}] : "
    read c
    if [[ "$c" -lt 1 ]] || [[ "$c" -gt "${#array[@]}" ]] ; then
        echo 1>&2 "Are you fucking stupid?"
        exit 1
    fi
    let c-=1
    NGPT_DIR=${array[$c]}
else
    NGPT_DIR=${array[0]}
fi

# Make sure we have a ngpt/doc folder
DOC_DIR=${NGPT_DIR}/doc
if ! test -d ${DOC_DIR} ; then
    echo 1>&2 "Could not find the /doc dir : \"$DOC_DIR\""
    exit 1
fi

# create the md file to convert
PAND_DOC=${DOC_DIR}/pandoc/pndc.md
> ${PAND_DOC}
if ! test -f "${DOC_DIR}/pandoc/meta.yaml" ; then
    echo 1>&2 "File \"${DOC_DIR}/pandoc/meta.yaml\" seems to be missing!"
else
    ## set the meta-file (yaml)
    dt=$(date -R)
    cat ${DOC_DIR}/pandoc/meta.yaml | sed "s/date:.*/date: ${dt}/g" > ${PAND_DOC}
    echo "Meta-file \"${DOC_DIR}/pandoc/meta.yaml\" pasted to \"${PAND_DOC}\""
fi

# Paste readme.md to /doc
if ! cat ${NGPT_DIR}/readme.md >> ${PAND_DOC} ; then
    echo 1>&2 "Failed to copy \"${NGPT_DIR}/readme.md\"."
    exit 1
fi

# customize the readme.md
sed -i "s|doc/figures/sep-various-antex-example.png|${DOC_DIR}/figures/sep-various-antex-example.png|g" \
    ${PAND_DOC}
# set the paths in ngpt-template.latex
sed -i "s|PATH_TO_DOC|${DOC_DIR}|g" ${DOC_DIR}/pandoc/ngpt-template.latex

if ! pandoc ${PAND_DOC} -o ${DOC_DIR}/readme.${1} \
        --from=markdown_github+simple_tables+table_captions+yaml_metadata_block \
        -s \
        --smart \
        --normalize \
        --standalone \
        --table-of-contents \
        --highlight-style=pygments \
        --number-sections \
        --listings \
        --template=${DOC_DIR}/pandoc/ngpt-template.${1} ; then
    echo 1>&2 "Failed to pandoc!"
    exit 1
fi

# refine output (if latex)
#if test "$1" == "latex" ; then
#    echo "Refining latex output."
#    if ! ${DOC_DIR}/pandoc/latex-alert-tokenize.py ${DOC_DIR}/readme.${1} > .tmp; then
#        echo 1>&2 "Failed to run \"latex-alert-tokenize.py\"."
#    else
#        mv .tmp ${DOC_DIR}/readme.${1}
#    fi
#fi

# output
if test "$?" -eq 0 ; then
    echo "Output file created: \"${DOC_DIR}/readme.${1}\"."
    echo "All done!"
    echo "Now run \"pdflatex ${DOC_DIR}/readme.${1}\" to compile the pdf."
else
    echo 1>&2 "Something went wrong!"
    exit 1
fi

exit 0
