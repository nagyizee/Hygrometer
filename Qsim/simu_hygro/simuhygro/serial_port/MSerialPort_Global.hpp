#ifndef MSERIALPORT_GLOBAL_HPP
#define MSERIALPORT_GLOBAL_HPP

// This header allows for qmake library support
// this code gets built into a DLL or SO file depending on platform

#include <QtCore/qglobal.h>

#if defined(MSERIALPORT_LIBRARY)
#  define MSERIALPORTSHARED_EXPORT Q_DECL_EXPORT
#else
#  define MSERIALPORTSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QSERIALPORT_GLOBAL_HPP
