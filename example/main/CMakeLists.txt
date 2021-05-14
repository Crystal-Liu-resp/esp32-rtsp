
set(_EMBED_FILES
"../images/frames/hd_0.jpg"
"../images/frames/hd_1.jpg"
"../images/frames/hd_2.jpg"
"../images/frames/hd_3.jpg"
"../images/frames/hd_4.jpg"
"../images/frames/hd_5.jpg"
"../images/frames/hd_6.jpg"
"../images/frames/hd_7.jpg"
"../images/frames/hd_8.jpg"
"../images/frames/hd_9.jpg")

idf_component_register(SRC_DIRS "."  "../../src"
                    INCLUDE_DIRS "."  "../../src"
                    PRIV_REQUIRES console spi_flash nvs_flash fatfs mdns esp_http_server
                    EMBED_TXTFILES ${_EMBED_FILES} )

