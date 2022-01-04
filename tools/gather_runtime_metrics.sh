# Script to gather runtime metrics on fstalign binary

benchmark_settings() {
    local outdir=$1
    local ref_length=$2
    local num_repeats=$3
    local ins_rate=$4
    local del_rate=$5
    local sub_rate=$6
    local outcsv=$7

    for i in $(seq $num_repeats); do
        perl generate_wer_test_data.pl --ins_fract $ins_rate \
            --del_fract $del_rate \
            --sub_fract $sub_rate \
            --ref_length $ref_length \
            --oref "${outdir}/ref${i}.txt" \
            --ohyp "${outdir}/hyp${i}.txt"

        /usr/bin/time -v fstalign wer --ref "${outdir}/ref${i}.txt" \
            --hyp "${outdir}/hyp${i}.txt" 2> "${outdir}/stats${i}.txt"

        runtime=$(grep "Elapsed (wall clock) time" "${outdir}/stats${i}.txt" | awk 'NF>1{print $NF}')
        ram=$(grep "Maximum resident set size" "${outdir}/stats${i}.txt" | awk 'NF>1{print $NF}')

        echo "${ref_length},${ins_rate},${del_rate},${sub_rate},${runtime},${ram}" >> "${outcsv}"
    done
}

main() {
    echo "$0 $@"  # Print the command line for logging

    local outcsv=$1

    echo "length,ins,del,sub,runtime,ram" >> "${outcsv}"

    # Stage 1: medium transcripts, different WER
    dir="temp"
    mkdir "${dir}"
    benchmark_settings "${dir}" 1000 5 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 1000 5 0.2 0.2 0.2 "${outcsv}"
    benchmark_settings "${dir}" 1000 5 0.3 0.3 0.3 "${outcsv}"
    benchmark_settings "${dir}" 1000 5 0.1 0.1 0.4 "${outcsv}"
    benchmark_settings "${dir}" 1000 5 0.1 0.4 0.1 "${outcsv}"
    benchmark_settings "${dir}" 1000 5 0.4 0.1 0.1 "${outcsv}"

    # Stage 2: single WER, different length transcripts
    benchmark_settings "${dir}" 100 10 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 200 10 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 400 10 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 800 5 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 2000 5 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 4000 2 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 8000 2 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 16000 2 0.1 0.1 0.1 "${outcsv}"
    benchmark_settings "${dir}" 32000 2 0.1 0.1 0.1 "${outcsv}"
}

main "$@"
