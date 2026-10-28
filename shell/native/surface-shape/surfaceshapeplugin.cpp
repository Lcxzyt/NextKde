#include "cursoroverride.h"
#include "surfaceshape.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickImageProvider>
#include <qqml.h>

QQuickImageProvider *createBackdropMapProvider();

class SurfaceShapePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<SurfaceShape>(uri, 1, 0, "SurfaceShape");
        qmlRegisterSingletonType<CursorOverride>(uri, 1, 0, "CursorOverride",
            [](QQmlEngine *, QJSEngine *) -> QObject * {
                return new CursorOverride;
            });
    }
    void initializeEngine(QQmlEngine *engine, const char *uri) override
    {
        QQmlExtensionPlugin::initializeEngine(engine, uri);
        if (!engine->imageProvider(QStringLiteral("kos-backdrop")))
            engine->addImageProvider(QStringLiteral("kos-backdrop"), createBackdropMapProvider());
    }
};

#include "surfaceshapeplugin.moc"
