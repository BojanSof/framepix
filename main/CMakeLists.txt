idf_component_register(
  SRCS "main.cpp" "WifiProvisioningWeb.cpp" "led_strip_encoder.c"
  INCLUDE_DIRS ""
  EMBED_FILES "web_interface/wifi_login.html" "web_interface/css/styles.css"
)
