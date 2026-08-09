add_executable(TestGameInferQueue
        gameinfer_queue_test.cpp
        ../apps/GameInfer/gui/InferenceQueueController.cpp
        ../apps/GameInfer/gui/InferenceQueueController.h
)
set_target_properties(TestGameInferQueue PROPERTIES AUTOMOC ON)
target_include_directories(TestGameInferQueue PRIVATE ../apps/GameInfer/gui)
target_link_libraries(TestGameInferQueue PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Concurrent
)
