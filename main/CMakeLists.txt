idf_component_register(
  SRCS
    "main.cpp"
    "WifiProvisioningWeb.cpp"
    "FramepixServer.cpp"
    "LedMatrix.cpp"
    "MatrixAnimator.cpp"
    "led_strip_encoder.c"
  INCLUDE_DIRS ""
  EMBED_FILES
    "web_interface/wifi_login.html"
    "web_interface/designer.html"
    "web_interface/css/styles.css"
    "web_interface/script.js"
)
