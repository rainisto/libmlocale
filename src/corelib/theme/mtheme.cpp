/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of libmeegotouch.
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "mtheme.h"
#include "mtheme_p.h"

#include "mlibrary.h"
M_LIBRARY

#ifdef Q_WS_X11
#include <QX11Info>
#endif
#include <QPixmap>
#include <MDebug>
#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QDir>

#include "private/mwidgetcontroller_p.h"

#ifdef HAVE_GCONF
#include "mgconfitem.h"
#endif

#include "mclassfactory.h"
#include "mstyle.h"
#include "mremotethemedaemon.h"
#include "mlocalthemedaemon.h"
#include "mstylesheet.h"
#include "mwidgetcontroller.h"
#include "mwidgetview.h"
#include "mscenemanager.h"
#include "mscene.h"
#include "mscalableimage.h"

#include "private/mwidgetcontroller_p.h"

#include "mapplication.h"
#include "mapplicationwindow.h"
#include "mcomponentdata.h"
#include "mcomponentdata_p.h"

// Must be last as it conflicts with some Qt defined types
#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

MTheme *gTheme = 0;

QHash<QString, MLibrary *>* MThemePrivate::libraries = NULL;
MThemePrivate::RegisteredStyleContainers MThemePrivate::styleContainers;

namespace
{
    // "default_pixmap_MyPixmap_47_47"
    static QString defaultPixmapCacheId(const QString &name, int width, int height)
    {
        return QString::fromLatin1("default_pixmap_") + name
                + QChar::fromLatin1('_') + QString::number(width)
                + QChar::fromLatin1('_') + QString::number(height);
    }

    // "scalable_image_myscalable_5_5_5_5
    static QString scalableImageCacheId(const QString &name, int left, int top, int right, int bottom)
    {
        return QString::fromLatin1("scalable_image_") + name
                + QChar::fromLatin1('_') + QString::number(left)
                + QChar::fromLatin1('_') + QString::number(top)
                + QChar::fromLatin1('_') + QString::number(right)
                + QChar::fromLatin1('_') + QString::number(bottom);
    }
} // anonymous namespace

MThemePrivate::LeakedStyles MThemePrivate::leakedStyles;

void mMessageHandler(QtMsgType type, const char *msg);

MThemePrivate::LeakedStyles::~LeakedStyles()
{
    // as LeakedStyles is a static class the method handler could not be valid
    // anymore at this point. work around this problem by creating a new one
    qInstallMsgHandler(mMessageHandler);

    QHash<MStyle*, QString>::iterator end = styles.end();
    for (QHash<MStyle*, QString>::iterator iterator = styles.begin();
            iterator != end;
            ++iterator) {
        MStyle *leak = iterator.key();
        QString id = iterator.value();

        mWarning("mtheme.cpp") << "Style:" << id << "not released!" << "refcount:" << leak->references();
    }
}

void MThemePrivate::addLeakedStyle(MStyle *style, const QString &id)
{
    leakedStyles.insert(style, id);
}

void MThemePrivate::removeLeakedStyle(MStyle *style)
{
    leakedStyles.remove(style);
}

MTheme::MTheme(const QString &applicationName, const QString &, ThemeService themeService) :
    d_ptr(new MThemePrivate(applicationName, themeService))
{
    if (gTheme || (MComponentData::instance() && MComponentData::instance()->d_ptr->theme))
        qFatal("There cannot be multiple instances of MTheme, use MTheme::instance() instead of constructing a new one");

    Q_D(MTheme);

    d->q_ptr = this;

    connect(d->themeDaemon, SIGNAL(themeChanged(QStringList, QStringList)),
            SLOT(themeChangedSlot(QStringList, QStringList)));

    connect(d->themeDaemon, SIGNAL(pixmapCreatedOrChanged(QString, QSize, Qt::HANDLE)),
            SLOT(pixmapCreatedOrChangedSlot(QString, QSize, Qt::HANDLE)));

    connect(d->themeDaemon, SIGNAL(themeChangeCompleted()), SIGNAL(themeChangeCompleted()));

#ifdef HAVE_GCONF
    connect(&d->locale, SIGNAL(valueChanged()), SLOT(localeChangedSlot()));
#endif

    gTheme = this;
}

MTheme::~MTheme()
{
    MStyleSheet::cleanup(false);

    QHash<QString, CachedScalableImage>::iterator i2 = d_ptr->scalableImageIdentifiers.begin();
    QHash<QString, CachedScalableImage>::iterator end2 = d_ptr->scalableImageIdentifiers.end();
    for (; i2 != end2; ++i2) {
        qWarning() << "MTheme - MScalableImage" << i2.key() << "not released!" << "refcount:" << i2.value().refcount;
        releasePixmap(i2.value().image->pixmap());
        delete i2.value().image;
    }


    // print identifiers from all pixmaps which were not released
    QHash<QString, CachedPixmap>::iterator i = d_ptr->pixmapIdentifiers.begin();
    QHash<QString, CachedPixmap>::iterator end = d_ptr->pixmapIdentifiers.end();
    for (; i != end; ++i) {
        qWarning() << "MTheme - pixmap" << i.key() << "not released!" << "refcount:" << i.value().refcount;
    }

    // unload all theme libraries
    d_ptr->reloadThemeLibraries(QStringList());

    gTheme = NULL;
    delete d_ptr;
}


bool MTheme::addPixmapDirectory(const QString &directoryName, M::RecursionMode recursive)
{
    QDir dir(directoryName);
    if (!dir.exists())
        return false;

    instance()->d_ptr->themeDaemon->addDirectoryToPixmapSearchList(dir.absolutePath(), recursive);
    return true;
}

void MTheme::clearPixmapDirectories()
{
    instance()->d_ptr->themeDaemon->clearPixmapSearchList();
}

MTheme *MTheme::instance()
{
    if (MComponentData::instance())
        return MComponentData::instance()->d_ptr->theme;

    if (!gTheme) {
        // This allows MTheme to be independent from MApplication.
        // Uses this process' name as the theme identifier by default
        QFileInfo fileInfo(QCoreApplication::instance()->applicationName());
        QString applicationName = fileInfo.fileName();
        gTheme = new MTheme(applicationName);
    }

    return gTheme;
}

const QPixmap *MTheme::pixmap(const QString &id, const QSize &size)
{
    if(id.isEmpty()) {
        mWarning("MTheme") << "requested pixmap without id";
        return instance()->d_ptr->invalidPixmap;
    }

    // TODO: check if needed
    QSize realSize = size;
    if (realSize.width() < 1)
        realSize.rwidth() = 0;
    if (realSize.height() < 1)
        realSize.rheight() = 0;

    QString identifier = defaultPixmapCacheId(id, realSize.width(), realSize.height());
    const QPixmap *p = instance()->d_ptr->fetchPixmapFromCache(identifier);

    // check if we found the pixmap from cache?
    if (p)
        return p;

    QPixmap *result = new QPixmap(realSize);
    if (instance()->d_ptr->showAsyncRequests) {
        result->fill(QColor(0, 255, 0, 255));
    } else {
        result->fill(QColor(0, 0, 0, 0));
    }

    instance()->d_ptr->pixmapIdentifiers.insert(identifier, CachedPixmap(result, id, realSize));
    instance()->d_ptr->themeDaemon->pixmapHandle(id, realSize);

    return result;
}

QPixmap *MTheme::pixmapCopy(const QString &id, const QSize &size)
{
    // TODO: check if needed
    QSize realSize = size;
    if (realSize.width() < 1)
        realSize.rwidth() = 0;
    if (realSize.height() < 1)
        realSize.rheight() = 0;

    // if there is no entry for the copied pixmap yet, we need to create it
    QString identifier = defaultPixmapCacheId(id, realSize.width(), realSize.height());
    const QPixmap *p = instance()->d_ptr->fetchPixmapFromCache(identifier);
    if (!p) {
        QPixmap *result = new QPixmap();
        instance()->d_ptr->pixmapIdentifiers.insert(identifier, CachedPixmap(result, id, realSize));
        p = result;
    }

    //TODO: check if the local pixmap pointer is valid already,
    //      no need to fetch anything from daemon then

    //force daemon to load the pixmap synchronously, then make copy of the
    //pixmap and release it immediately
    instance()->d_ptr->themeDaemon->pixmapHandleSync(id, realSize);
    QPixmap* copy = new QPixmap(p->copy());
    releasePixmap(p);
    return copy;
}

const MScalableImage *MTheme::scalableImage(const QString &id, int left, int right, int top, int bottom)
{
    // check if we already have this scalable image in the cache
    QString scalableidentifier = scalableImageCacheId(id, left, top, right, bottom);
    QHash<QString, CachedScalableImage>::iterator i = instance()->d_ptr->scalableImageIdentifiers.find(scalableidentifier);
    if (i != instance()->d_ptr->scalableImageIdentifiers.end()) {
        //image found, increase refcount and return it
        i.value().refcount.ref();
        return i.value().image;
    }

    //first try to fetch the used pixmap from the cache
    QString pixmapidentifier = defaultPixmapCacheId(id, 0, 0);
    const QPixmap *p = instance()->d_ptr->fetchPixmapFromCache(pixmapidentifier);
    if (!p) {
        // we have to create temporary pixmap
        QPixmap *result = new QPixmap(1, 1);
        if (instance()->d_ptr->showAsyncRequests) {
            result->fill(QColor(0, 255, 0, 255));
        } else {
            result->fill(QColor(0, 0, 0, 0));
        }

        instance()->d_ptr->pixmapIdentifiers.insert(pixmapidentifier, CachedPixmap(result, id, QSize(0, 0)));
        instance()->d_ptr->themeDaemon->pixmapHandle(id, QSize(0, 0));

        p = result;
    }

    //create the actual scalable image and cache it
    MScalableImage *image = new MScalableImage(p, left, right, top, bottom, id);
    instance()->d_ptr->scalableImageIdentifiers.insert(scalableidentifier, CachedScalableImage(image));

    return image;
}

void MTheme::releaseScalableImage(const MScalableImage *image)
{
    // find the image from cache and decrease refcount + release if refcount = 0
    // TODO: this could be optimized
    QHash<QString, CachedScalableImage>::iterator i = instance()->d_ptr->scalableImageIdentifiers.begin();
    QHash<QString, CachedScalableImage>::iterator end = instance()->d_ptr->scalableImageIdentifiers.end();
    for (; i != end; ++i) {
        // is this the image which we should release?
        if (i.value().image == image) {
            if (!i.value().refcount.deref()) {
                releasePixmap(i.value().image->pixmap());
                delete i.value().image;
                instance()->d_ptr->scalableImageIdentifiers.erase(i);
            }
            break;
        }
    }
}


void MTheme::releasePixmap(const QPixmap *pixmap)
{
    // NULL pixmap, do nothing
    if (!pixmap)
        return;

    // invalidPixmap, no need to release it
    if (pixmap == instance()->d_ptr->invalidPixmap) {
        return;
    }

    // find the pixmap from cache and decrease refcount + release if refcount = 0
    // TODO: this could be optimized
    QHash<QString, CachedPixmap>::iterator i = instance()->d_ptr->pixmapIdentifiers.begin();
    QHash<QString, CachedPixmap>::iterator end = instance()->d_ptr->pixmapIdentifiers.end();
    for (; i != end; ++i) {       // is this the pixmap which we should release?
        if (i.value().pixmap == pixmap) {
            if (!i.value().refcount.deref()) {
                instance()->d_ptr->themeDaemon->releasePixmap(i.value().imageId, i.value().size);
                delete i.value().pixmap;
                instance()->d_ptr->pixmapIdentifiers.erase(i);
            }
            return;
        }
    }

    // check if we didn't find the pixmap from our cache
    Q_ASSERT_X(false, "MTheme::releasePixmap", "Pixmap not found from the cache!");
}

const MStyle *MTheme::style(const char *styleClassName,
                                const QString &objectName)
{
    return MTheme::style(styleClassName, objectName, 0, 0, M::Landscape, NULL);
}

const MStyle *MTheme::style(const char *styleClassName,
                                const QString &objectName,
                                const QString &mode,
                                const QString &type,
                                M::Orientation orientation,
                                const MWidgetController *parent)
{
    // The style type should never be "default" - that would probably be a view type
    // that's mistakenly being used as a style type.
    // The caller probably means "" instead.
    Q_ASSERT(type != "default");

    MThemePrivate *d = MTheme::instance()->d_func();

    // list containing all stylesheets from all assemblies from which this style is/inherits + app css
    QList<const MStyleSheet *> sheets;

    // go trough the inheritance chain and add stylesheets from each assembly.
    const QMetaObject *mobj = MClassFactory::instance()->styleMetaObject(styleClassName);
    do {
        M::AssemblyType assemblyType = MClassFactory::instance()->styleAssemblyType(mobj->className());
        if (assemblyType == M::Application) {
            mobj = mobj->superClass();
            continue;
        }
        QString assemblyName = MClassFactory::instance()->styleAssemblyName(mobj->className());

        // find proper library
        MLibrary *library = d->libraries->value(assemblyName, NULL);
        if (library) {
            // use stylesheet from this library if there is one
            if (library->stylesheet()) {
                if (!sheets.contains(library->stylesheet())) {
                    sheets.insert(0, library->stylesheet());
                }
            }
        } else {
            mWarning("MTheme") << "Cannot find library. You must register your library to theming using M_LIBRARY macro." << '(' << assemblyName << ')';
            Q_ASSERT_X(library, "MTheme", "Failed to find library");
        }
        mobj = mobj->superClass();
    } while (mobj->className() != QObject::staticMetaObject.className());

    const QGraphicsItem* p = parent;
    QList<QPair<const QMetaObject*, QList<const MStyleSheet*> > > parentsSheets;
    // go trough the parents and add them to parent stylesheet list
    while (p) {
        if (p->isWidget()) {
            const QGraphicsWidget* widget = static_cast<const QGraphicsWidget*>(p);

            mobj = widget->metaObject();
            QList<const MStyleSheet*> thisParentSheets;
            // go trough this parent's inheritance chain
            do {
                M::AssemblyType assemblyType = MClassFactory::instance()->widgetAssemblyType(mobj->className());
                if (assemblyType == M::Application || assemblyType == M::AssemblyNone) {
                    mobj = mobj->superClass();
                    continue;
                }
                QString assemblyName = MClassFactory::instance()->widgetAssemblyName(mobj->className());
                MLibrary *library = d->libraries->value(assemblyName, NULL);
                if (library) {
                    if (library->stylesheet()) {
                        if (!sheets.contains(library->stylesheet()) && !thisParentSheets.contains(library->stylesheet())) {
                            thisParentSheets.insert(0, library->stylesheet());
                        }
                    }
                }
                mobj = mobj->superClass();
            } while (mobj->className() != QObject::staticMetaObject.className());
            parentsSheets.insert(0, qMakePair(widget->metaObject(), thisParentSheets));
        }

        p = p->parentItem();
    }

    // add application css
    if (d->application->stylesheet())
        sheets.append(d->application->stylesheet());

    // add custom stylesheet
    if (d->customStylesheet)
        sheets.append(d->customStylesheet);

    return MStyleSheet::style(sheets, parentsSheets, styleClassName, objectName, mode, type, orientation, parent);
}

void MTheme::releaseStyle(const MStyle *style)
{
    if (!style)
        return;

    MStyleSheet::releaseStyle(style);
}

QAbstractAnimation *MTheme::animation(const QString &animationTypeName)
{
    QAbstractAnimation *a = MClassFactory::instance()->createAnimation(animationTypeName);
    if (!a) {
        qWarning() << "Failed to create implementation for: " << animationTypeName;
        return NULL;
    }

    return a;
}

MWidgetView *MTheme::view(const MWidgetController *controller)
{
    // Best matching view class name
    QString viewClassName = instance()->d_ptr->determineViewClassForController(controller);

    if (viewClassName.isEmpty()) {
        qWarning() << "Could not find view class for:" << controller->metaObject()->className() << "/" << controller->viewType();
        return NULL;
    }

    MWidgetView *v = MClassFactory::instance()->createView(viewClassName.toStdString().c_str(), controller);
    if (!v) {
        qWarning() << "Failed to create view for:" << controller->metaObject()->className() << "/" << controller->viewType() << ".  Class name found was: " << viewClassName;
        return NULL;
    }

    return v;
}

const MPalette &MTheme::palette()
{
    return instance()->d_ptr->palette;
}

const MDefaultFonts &MTheme::fonts()
{
    return instance()->d_ptr->fonts;
}

bool MTheme::loadCSS(const QString &filename, InsertMode mode)
{
    if (instance()->d_ptr->loadCSS(filename, mode)) {
        // Re-populate all the styles, custom stylesheet may have overridden something
        for (MThemePrivate::RegisteredStyleContainers::iterator iterator = MThemePrivate::styleContainers.begin(); iterator != MThemePrivate::styleContainers.end(); ++iterator) {
            iterator.value()->reloadStyles();
        }
        // notify all widgets that style needs to be applied
        QSet<MWidgetController *>::iterator end = MWidgetControllerPrivate::allSystemWidgets.end();
        for (QSet<MWidgetController *>::iterator i = MWidgetControllerPrivate::allSystemWidgets.begin();
                i != end; ++i) {
            // get view ask it to apply the new style
            const MWidgetView *view = (*i)->view();
            if (view) {
                const_cast<MWidgetView *>(view)->applyStyle();
            }
        }
        return true;
    }

    return false;
}

bool MThemePrivate::loadCSS(const QString &filename, MTheme::InsertMode mode)
{
    MStyleSheet *newStylesheet = new MStyleSheet(&logicalValues);
    bool result = newStylesheet->load(filename);
    if (result) {
        // loading ok, check what to do.. overwrite or append?
        if ((mode == MTheme::Append) && customStylesheet) {
            // append the loaded style data into the existing stylesheet
            *customStylesheet += *newStylesheet;
            delete newStylesheet;
            newStylesheet = NULL;
        } else {
            // no existing stylesheet or the stylesheet needs to be overwritten
            delete customStylesheet;
            customStylesheet = newStylesheet;
        }

        // Cached entries are not valid any more
        MStyleSheet::cleanup(false);
        return true;
    }

    delete newStylesheet;
    return false;
}

QString MTheme::currentTheme()
{
    MThemePrivate *d = MTheme::instance()->d_func();
    return d->themeDaemon->currentTheme();
}

bool MTheme::hasPendingRequests()
{
    return instance()->d_ptr->themeDaemon->hasPendingRequests();
}

void MThemePrivate::reinit(const QString &newApplicationName)
{
    delete application;
    applicationName = newApplicationName;
    application = new MAssembly(applicationName);
    themeDaemon->registerApplicationName(newApplicationName);
    application->themeChanged(themeDaemon->themeInheritanceChain(), logicalValues);
}

MThemePrivate::MThemePrivate(const QString &applicationName, MTheme::ThemeService themeService) :
    applicationName(applicationName),
    customStylesheet(NULL),
    application(new MAssembly(applicationName)),
    palette(logicalValues),
    fonts(logicalValues)
#ifdef HAVE_GCONF
    , locale("/meegotouch/i18n/language")
    , showAsyncRequestsItem("/meegotouch/debug/show_async_requests")
#endif
    , showAsyncRequests(false)
{
    switch (themeService) {
    case MTheme::LocalTheme:
        themeDaemon = new MLocalThemeDaemon(applicationName);
        break;

    case MTheme::RemoteTheme:
        themeDaemon = new MRemoteThemeDaemon(applicationName, -1);
        break;

    case MTheme::AnyTheme: {
        MRemoteThemeDaemon *tds = new MRemoteThemeDaemon(applicationName, 0);
        if (tds->connected()) {
            themeDaemon = tds;
        } else {
            delete tds;
            themeDaemon = new MLocalThemeDaemon(applicationName);
        }
    } break;
    }

    invalidPixmap = new QPixmap(50, 50);
    invalidPixmap->fill(QColor(255, 64, 64, 255));

    // this loads the current theme
    reloadThemeLibraries(themeDaemon->themeLibraryNames());
    refreshLocalThemeConfiguration(themeDaemon->themeInheritanceChain());

#ifdef HAVE_GCONF
    showAsyncRequests = showAsyncRequestsItem.value(false).toBool();
    connect(&showAsyncRequestsItem, SIGNAL(valueChanged()), this, SLOT(updateShowAsyncRequests()));
#endif
}

MThemePrivate::~MThemePrivate()
{
    delete application;
    delete themeDaemon;
    delete invalidPixmap;
    delete customStylesheet;

    // TODO: check if we need to release QPixmaps from pixmapHandles
}

QString MThemePrivate::determineViewClassForController(const MWidgetController *controller)
{
    bool exactMatch = false;

    // first search from application view configuration
    QString bestMatch = application->viewType(controller, exactMatch);
    if (exactMatch)
        return bestMatch;

    for (const QMetaObject *metaObject = controller->metaObject(); metaObject != &MWidget::staticMetaObject; metaObject = metaObject->superClass()) {

        // check if this widget is declared inside some library
        M::AssemblyType type = MClassFactory::instance()->widgetAssemblyType(metaObject->className());
        if (type == M::Application)
            continue;

        // get name of the library where this widget was declared
        QString assemblyName = MClassFactory::instance()->widgetAssemblyName(metaObject->className());
        if (assemblyName.isEmpty())
            continue;

        // find proper library
        MLibrary *library = libraries->value(assemblyName, NULL);
        Q_ASSERT_X(library, "MTheme", "Failed to find library");

        // try to get view type for the widget
        QString viewClassName = library->viewType(controller, exactMatch);
        if (exactMatch) {
            return viewClassName;
        }

        if (bestMatch.isEmpty()) {
            bestMatch = viewClassName;
        }
    }

    return bestMatch;
}

const QPixmap *MThemePrivate::fetchPixmapFromCache(const QString &identifier)
{
    QHash<QString, CachedPixmap>::iterator i = pixmapIdentifiers.find(identifier);

    // check if we already have this pixmap in cache
    if (i != pixmapIdentifiers.end()) {
        i.value().refcount.ref();
        return i.value().pixmap;
    }

    // if not return null
    return NULL;
}

void MThemePrivate::themeChangedSlot(const QStringList &themeInheritance, const QStringList& libraryNames)
{
    refreshLocalThemeConfiguration(themeInheritance);
    q_ptr->rebuildViewsForWidgets();
    reloadThemeLibraries(libraryNames);
    emit q_ptr->themeIsChanging();
}

void MThemePrivate::refreshLocalThemeConfiguration(const QStringList &themeInheritance)
{
    QString language;

#ifdef HAVE_GCONF
    // determine current language
    language = locale.value("en_GB").toString();
#endif

    // Load logical values from ini file
    logicalValues.load(themeInheritance, language);

    // refresh default fonts & palette
    palette.refresh();
    fonts.refresh();

    // load all css-files from all libraries from all themes.
    foreach(MLibrary * lib, *libraries) {
        lib->themeChanged(themeInheritance, logicalValues);
    }

    // refresh application theme data
    application->themeChanged(themeInheritance, logicalValues);

    // cached data is no more valid
    MStyleSheet::cleanup(false);
}

void MThemePrivate::reloadThemeLibraries(const QStringList& libraryNames)
{
    QString libsuffix;

#ifdef Q_OS_WIN
    // under windows the libraries are suffixed with a "0",
    // e.g. meegotouchviews0.dll, so the 0 here is needed,
    // so that the library can be loaded under windows.
    libsuffix = "0";
#endif

    // store list of libraries that needs to be unloaded
    QSet<QLibrary*> toUnload = openedThemeLibraries;

    // load all new libraries (if the library is already loaded, it will ref the loaded one)
    openedThemeLibraries.clear();
    foreach(const QString& libname, libraryNames) {
        QLibrary* library = new QLibrary(libname + libsuffix);
        if(library->load()) {
            openedThemeLibraries.insert(library);
        } else {
            mWarning("MTheme") << "Failed to open theme library:" << libname;
            delete library;
        }
    }

    // unload old themelibraries
    foreach(QLibrary* library, toUnload) {
        library->unload();
        delete library;
    }
}

void MThemePrivate::registerLibrary(MLibrary *library)
{
    if (!MThemePrivate::libraries)
        MThemePrivate::libraries = new QHash<QString, MLibrary *>();

    MThemePrivate::libraries->insert(library->name(), library);

    // Load theme-specific content of this library (in case the lib was loaded after startup).
    // TODO: load/unload the stylesheets of this library, this only handles view configuration atm.
    if (gTheme) {
        MThemePrivate *d = MTheme::instance()->d_ptr;
        library->themeChanged(d->themeDaemon->themeInheritanceChain(), d->logicalValues);
    }
}

void MThemePrivate::unregisterLibrary(MLibrary *library)
{
    MThemePrivate::libraries->remove(library->name());

    if (MThemePrivate::libraries->count() == 0) {
        delete MThemePrivate::libraries;
        MThemePrivate::libraries = NULL;
    }
}

void MThemePrivate::pixmapRequestFinished()
{
    Q_Q(MTheme);

    if (!MTheme::hasPendingRequests()) {
        if (MApplication::activeWindow() && MApplication::activeWindow()->viewport()) {
            MApplication::activeWindow()->viewport()->update();
        }
        emit q->pixmapRequestsFinished();
    }
}

void MThemePrivate::pixmapCreatedOrChangedSlot(const QString &imageId, const QSize &size, Qt::HANDLE pixmapHandle)
{
    QString identifier = defaultPixmapCacheId(imageId, size.width(), size.height());
    QHash<QString, CachedPixmap>::iterator iterator = pixmapIdentifiers.find(identifier);

    if (iterator == pixmapIdentifiers.end()) {
        // the updated pixmap has already been released, but the daemon didn't get the message early enough
        return;
    }

    QPixmap *pixmap = (QPixmap *) iterator.value().pixmap;

    if (pixmapHandle == 0) {
        mWarning("MThemePrivate") << "pixmapChangedSlot - pixmap reload failed (null handle):" << identifier;
        *pixmap = *invalidPixmap;

        pixmapRequestFinished();
        return;
    }

#ifdef Q_WS_X11
    // we can create the real pixmap right now
    *pixmap = QPixmap::fromX11Pixmap(pixmapHandle, QPixmap::ImplicitlyShared);
#else
    QPixmap *pixmapPointer = (QPixmap *)(pixmapHandle);
    *pixmap = *pixmapPointer;
#endif

    pixmapRequestFinished();
}


void MTheme::rebuildViewsForWidgets()
{
    Q_D(MTheme);

    // Re-populate all the styles
    // TODO: This could be probably optimized somehow
    for (MThemePrivate::RegisteredStyleContainers::iterator iterator = d->styleContainers.begin(); iterator != d->styleContainers.end(); ++iterator) {
        iterator.value()->reloadStyles();
    }

    // go trough all widgets, replace views
    QSet<MWidgetController *>::iterator end = MWidgetControllerPrivate::allSystemWidgets.end();
    for (QSet<MWidgetController *>::iterator i = MWidgetControllerPrivate::allSystemWidgets.begin();
            i != end; ++i) {

        MWidgetController *controller = (*i);

        // figure out the view class name
        QString className = d->determineViewClassForController(controller);

        const MWidgetView *view = controller->view();
        if (view) {
            // check if it's completely new view class for this widget?
            if (className != view->metaObject()->className()) {
                controller->d_func()->deprecateView();
            } else {
                const_cast<MWidgetView *>(view)->applyStyle();
            }
        }
    }
}

void MThemePrivate::registerStyleContainer(MStyleContainer *container)
{
    styleContainers[container] = container;
}

void MThemePrivate::unregisterStyleContainer(MStyleContainer *container)
{
    RegisteredStyleContainers::iterator iterator = styleContainers.find(container);
    if (iterator != styleContainers.end()) {
        styleContainers.erase(iterator);
    }
}

void MThemePrivate::localeChangedSlot()
{
    themeChangedSlot(themeDaemon->themeInheritanceChain(), themeDaemon->themeLibraryNames());
}

#ifdef HAVE_GCONF
void MThemePrivate::updateShowAsyncRequests()
{
    showAsyncRequests = showAsyncRequestsItem.value().toBool();
}
#endif

#include "moc_mtheme.cpp"
