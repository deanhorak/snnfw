# Test CMakeLists.txt
find_package(GTest QUIET)

if(GTest_FOUND)
    # Test executables
    add_executable(test_neuron test_neuron.cpp)
    target_link_libraries(test_neuron
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_cluster test_cluster.cpp)
    target_link_libraries(test_cluster
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_threading test_threading.cpp)
    target_link_libraries(test_threading
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_neural_components test_neural_components.cpp)
    target_link_libraries(test_neural_components
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_spike_processor test_spike_processor.cpp)
    target_link_libraries(test_spike_processor
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_neural_object_factory test_neural_object_factory.cpp)
    target_link_libraries(test_neural_object_factory
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_hierarchical_structures test_hierarchical_structures.cpp)
    target_link_libraries(test_hierarchical_structures
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_datastore test_datastore.cpp)
    target_link_libraries(test_datastore
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_adapters AdapterTests.cpp)
    target_link_libraries(test_adapters
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(test_spike_propagation test_spike_propagation.cpp)
    target_link_libraries(test_spike_propagation
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    # Note: test_main.cpp disabled until Main class is re-implemented
    # add_executable(test_main test_main.cpp)
    # target_link_libraries(test_main
    #     ${PROJECT_NAME}
    #     GTest::gtest
    #     GTest::gtest_main
    # )

    # Add tests
    add_test(NAME NeuronTests COMMAND test_neuron)
    add_test(NAME ClusterTests COMMAND test_cluster)
    add_test(NAME ThreadingTests COMMAND test_threading)
    add_test(NAME NeuralComponentsTests COMMAND test_neural_components)
    add_test(NAME SpikeProcessorTests COMMAND test_spike_processor)
    add_test(NAME NeuralObjectFactoryTests COMMAND test_neural_object_factory)
    add_test(NAME HierarchicalStructuresTests COMMAND test_hierarchical_structures)
    add_test(NAME DatastoreTests COMMAND test_datastore)
    add_test(NAME AdapterTests COMMAND test_adapters)
    add_test(NAME SpikePropagationTests COMMAND test_spike_propagation)
    # add_test(NAME MainTests COMMAND test_main)
else()
    message(STATUS "Google Test not found. Skipping tests.")
endif()
