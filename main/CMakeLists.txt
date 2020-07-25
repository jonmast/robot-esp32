idf_component_register(SRCS "robot_esp32_main.c" "motor.c" "wifi.c" "server.c"
                    "controller.c" "ultrasonic.c"
                    INCLUDE_DIRS ""
                    EMBED_FILES "../frontend/remote.html")
