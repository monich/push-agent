TEMPLATE = app
TARGET = push-agent
CONFIG -= qt
CONFIG += link_pkgconfig
PKGCONFIG += gio-unix-2.0 gio-2.0 glib-2.0 dbus-1 libwspcodec
DBUS_SPEC_DIR = $$_PRO_FILE_PWD_/../spec
QMAKE_CFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers

SOURCES += \
  main.c \
  pa.c \
  pa_dir.c \
  pa_log.c \
  pa_ofono.c
HEADERS += \
  pa.h \
  pa_dir.h \
  pa_log.h \
  pa_ofono.h
OTHER_FILES += \
  $$DBUS_SPEC_DIR/org.ofono.Manager.xml \
  $$DBUS_SPEC_DIR/org.ofono.Modem.xml \
  $$DBUS_SPEC_DIR/org.ofono.PushNotification.xml \
  $$DBUS_SPEC_DIR/org.ofono.PushNotificationAgent.xml \
  $$DBUS_SPEC_DIR/org.ofono.SimManager.xml

CONFIG(debug, debug|release) {
    DEFINES += DEBUG
    DESTDIR = $$_PRO_FILE_PWD_/../build/debug
} else {
    DESTDIR = $$_PRO_FILE_PWD_/../build/release
}

# org.ofono.Manager
MANAGER_XML = $$DBUS_SPEC_DIR/org.ofono.Manager.xml
MANAGER_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.Manager $$MANAGER_XML
MANAGER_H = org.ofono.Manager.h
org_ofono_Manager_h.input = MANAGER_XML
org_ofono_Manager_h.output = $$MANAGER_H
org_ofono_Manager_h.commands = $$MANAGER_GENERATE
org_ofono_Manager_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Manager_h

MANAGER_C = org.ofono.Manager.c
org_ofono_Manager_c.input = MANAGER_XML
org_ofono_Manager_c.output = $$MANAGER_C
org_ofono_Manager_c.commands = $$MANAGER_GENERATE
org_ofono_Manager_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Manager_c
GENERATED_SOURCES += $$MANAGER_C

# org.ofono.Modem
MODEM_XML = $$DBUS_SPEC_DIR/org.ofono.Modem.xml
MODEM_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.Modem $$MODEM_XML
MODEM_H = org.ofono.Modem.h
org_ofono_Modem_h.input = MODEM_XML
org_ofono_Modem_h.output = $$MODEM_H
org_ofono_Modem_h.commands = $$MODEM_GENERATE
org_ofono_Modem_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Modem_h

MODEM_C = org.ofono.Modem.c
org_ofono_Modem_c.input = MODEM_XML
org_ofono_Modem_c.output = $$MODEM_C
org_ofono_Modem_c.commands = $$MODEM_GENERATE
org_ofono_Modem_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_Modem_c
GENERATED_SOURCES += $$MODEM_C

# org.ofono.PushNotification
PUSH_NOTIFICATION_XML = $$DBUS_SPEC_DIR/org.ofono.PushNotification.xml
PUSH_NOTIFICATION_H = org.ofono.PushNotification.h
org_ofono_pushnotification_h.input = PUSH_NOTIFICATION_XML
org_ofono_pushnotification_h.output = $$PUSH_NOTIFICATION_H
org_ofono_pushnotification_h.commands = gdbus-codegen --generate-c-code \
  org.ofono.PushNotification $$PUSH_NOTIFICATION_XML
org_ofono_pushnotification_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_pushnotification_h

PUSH_NOTIFICATION_C = org.ofono.PushNotification.c
org_ofono_pushnotification_c.input = PUSH_NOTIFICATION_XML
org_ofono_pushnotification_c.output = $$PUSH_NOTIFICATION_C
org_ofono_pushnotification_c.commands = gdbus-codegen --generate-c-code \
  org.ofono.PushNotification $$PUSH_NOTIFICATION_XML
org_ofono_pushnotification_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_pushnotification_c
GENERATED_SOURCES += $$PUSH_NOTIFICATION_C

# org.ofono.PushNotificationAgent
PUSH_NOTIFICATION_AGENT_XML = $$DBUS_SPEC_DIR/org.ofono.PushNotificationAgent.xml
PUSH_NOTIFICATION_AGENT_H = org.ofono.PushNotificationAgent.h
org_ofono_pushnotificationagent_h.input = PUSH_NOTIFICATION_AGENT_XML
org_ofono_pushnotificationagent_h.output = $$PUSH_NOTIFICATION_AGENT_H
org_ofono_pushnotificationagent_h.commands = gdbus-codegen --generate-c-code \
  org.ofono.PushNotificationAgent $$PUSH_NOTIFICATION_AGENT_XML
org_ofono_pushnotificationagent_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_pushnotificationagent_h

PUSH_NOTIFICATION_AGENT_C = org.ofono.PushNotificationAgent.c
org_ofono_pushnotificationagent_c.input = PUSH_NOTIFICATION_AGENT_XML
org_ofono_pushnotificationagent_c.output = $$PUSH_NOTIFICATION_AGENT_C
org_ofono_pushnotificationagent_c.commands = gdbus-codegen --generate-c-code \
  org.ofono.PushNotificationAgent $$PUSH_NOTIFICATION_AGENT_XML
org_ofono_pushnotificationagent_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_pushnotificationagent_c
GENERATED_SOURCES += $$PUSH_NOTIFICATION_AGENT_C

# org.ofono.SimManager
SIM_MANAGER_XML = $$DBUS_SPEC_DIR/org.ofono.SimManager.xml
SIM_MANAGER_GENERATE = gdbus-codegen --generate-c-code \
  org.ofono.SimManager $$SIM_MANAGER_XML
SIM_MANAGER_H = org.ofono.SimManager.h
org_ofono_SimManager_h.input = SIM_MANAGER_XML
org_ofono_SimManager_h.output = $$SIM_MANAGER_H
org_ofono_SimManager_h.commands = $$SIM_MANAGER_GENERATE
org_ofono_SimManager_h.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_SimManager_h

SIM_MANAGER_C = org.ofono.SimManager.c
org_ofono_SimManager_c.input = SIM_MANAGER_XML
org_ofono_SimManager_c.output = $$SIM_MANAGER_C
org_ofono_SimManager_c.commands = $$SIM_MANAGER_GENERATE
org_ofono_SimManager_c.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += org_ofono_SimManager_c
GENERATED_SOURCES += $$SIM_MANAGER_C
