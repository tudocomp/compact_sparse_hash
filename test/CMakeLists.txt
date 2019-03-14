if(NOT GTEST_FOUND)
    MESSAGE(STATUS "gtest is not available - tests disabled!")
    return()
endif()

include(tdc_testsuite)

run_test(compact_sparse_hash_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(compact_hash_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(compact_sparse_hash_displacement_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(compact_hash_displacement_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(compact_sparse_hash_elias_displacement_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(compact_hash_elias_displacement_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)

run_test(v2_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(sandbox_test DEPS ${TDC_TEST_DEPS} compact_sparse_hash)

run_test(compact_sparse_hashset_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(compact_sparse_hashset_displacement_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
run_test(compact_sparse_hashset_elias_displacement_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)

run_test(compact_sparse_hashset_serialization_tests DEPS ${TDC_TEST_DEPS} compact_sparse_hash)
