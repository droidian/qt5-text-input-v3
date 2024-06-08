TEMPLATE = lib
QT += core core-private gui gui-private waylandclient waylandclient-private

CONFIG += c++17 link_pkgconfig wayland-scanner unversioned_libname unversioned_soname

INCLUDEPATH += src \
    $$[QT_INSTALL_HEADERS]/QtWaylandClient/$$[QT_VERSION]/QtWaylandClient/private

SOURCES += \
    src/qwaylandinputmethodeventbuilder.cpp \
    src/qwaylandtextinputv3.cpp \
    src/waylandtextinputv3context.cpp \
    src/waylandtextinputv3contextplugin.cpp \

HEADERS += \
    src/qwaylandinputmethodeventbuilder_p.h \
    src/qwaylandtextinputv3_p.h \
    src/waylandtextinputv3context_p.h \
    src/waylandtextinputv3contextplugin.h \

WAYLANDCLIENTSOURCES += \
    src/protocols/text-input-unstable-v3.xml \

target.path = $$[QT_INSTALL_PLUGINS]/platforminputcontexts
INSTALLS += target
