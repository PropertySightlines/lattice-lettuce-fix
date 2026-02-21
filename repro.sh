for i in {1..20}; do
    ./scripts/run_test.sh tests/test_combinators.salt > log.txt 2>&1
    if [ $? -ne 0 ]; then
        echo "FAILED!"
        cat log.txt
        break
    fi
done
