#pragma once

#include <QList>
#include <QColor>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QElapsedTimer>
#include <QTimer>
#include "backdropink.h"

class QQuickWindow;
struct kos_surface_shape_v1;
struct wl_surface;

class SurfaceShape : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool contrastFeedback READ contrastFeedback WRITE setContrastFeedback NOTIFY finishChanged)
    Q_PROPERTY(bool contrastOnly READ contrastOnly WRITE setContrastOnly NOTIFY finishChanged)
    Q_PROPERTY(qreal backdropLuminance READ backdropLuminance NOTIFY backdropLuminanceChanged)
    // Protocol v10: per-cell brightness behind the shape, as an image URL a
    // ShaderEffect can sample ("" until the first map, or below v10).
    Q_PROPERTY(QString backdropMapSource READ backdropMapSource NOTIFY backdropMapChanged)
    Q_PROPERTY(QString backdropInkMapSource READ backdropInkMapSource NOTIFY backdropMapChanged)
    Q_PROPERTY(bool backdropInkUniform READ backdropInkUniform NOTIFY backdropMapChanged)
    Q_PROPERTY(bool backdropInkDark READ backdropInkDark NOTIFY backdropMapChanged)
    Q_PROPERTY(QSize backdropMapSize READ backdropMapSize NOTIFY backdropMapChanged)
    Q_PROPERTY(bool continuousBlurSupported READ continuousBlurSupported NOTIFY activeChanged)
    Q_PROPERTY(qreal blurSigma READ blurSigma WRITE setBlurSigma NOTIFY finishChanged)
    Q_PROPERTY(int effectType READ effectType WRITE setEffectType NOTIFY finishChanged)
    Q_PROPERTY(int material READ material WRITE setMaterial NOTIFY finishChanged)
    Q_PROPERTY(int tintMode READ tintMode WRITE setTintMode NOTIFY finishChanged)
    Q_PROPERTY(QColor tintColor READ tintColor WRITE setTintColor NOTIFY finishChanged)
    Q_PROPERTY(qreal tintOpacity READ tintOpacity WRITE setTintOpacity NOTIFY finishChanged)
    Q_PROPERTY(QQuickItem *target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(qreal exponent READ exponent WRITE setExponent NOTIFY exponentChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QRectF captureGeometry READ captureGeometry WRITE setCaptureGeometry NOTIFY captureGeometryChanged)
    Q_PROPERTY(bool fixedCaptureSupported READ fixedCaptureSupported NOTIFY activeChanged)
    Q_PROPERTY(qreal materialOpacity READ materialOpacity WRITE setMaterialOpacity NOTIFY materialOpacityChanged)
    Q_PROPERTY(bool materialOpacitySupported READ materialOpacitySupported NOTIFY activeChanged)
    Q_PROPERTY(bool revealEnabled READ revealEnabled WRITE setRevealEnabled NOTIFY revealChanged)
    Q_PROPERTY(bool revealOpened READ revealOpened WRITE setRevealOpened NOTIFY revealChanged)
    Q_PROPERTY(int revealDuration READ revealDuration WRITE setRevealDuration NOTIFY revealChanged)
    Q_PROPERTY(bool revealSupported READ revealSupported NOTIFY activeChanged)
    // Contrast scrim sent over protocol v3. scrimTint 0/1 = black/white;
    // scrimDecay > 1 selects fixed mode.
    Q_PROPERTY(bool scrimEnabled READ scrimEnabled WRITE setScrimEnabled NOTIFY scrimEnabledChanged)
    Q_PROPERTY(int scrimTint READ scrimTint WRITE setScrimTint NOTIFY scrimTintChanged)
    Q_PROPERTY(qreal scrimCap READ scrimCap WRITE setScrimCap NOTIFY scrimCapChanged)
    Q_PROPERTY(qreal scrimDecay READ scrimDecay WRITE setScrimDecay NOTIFY scrimDecayChanged)
    // Blur strength override sent over protocol v4. blurEnabled=false keeps the
    // shape on the window's default blur pipeline; blurLevel is the compositor
    // blur level 1..15 (the global kwinrc BlurStrength scale).
    Q_PROPERTY(bool blurEnabled READ blurEnabled WRITE setBlurEnabled NOTIFY blurEnabledChanged)
    Q_PROPERTY(int blurLevel READ blurLevel WRITE setBlurLevel NOTIFY blurLevelChanged)

public:
    enum Effect { Liquid, Frosted }; Q_ENUM(Effect)
    enum Material { Clear, Regular, Thick }; Q_ENUM(Material)
    enum TintMode { NoTint, Fixed, Graphite, Pearl }; Q_ENUM(TintMode)
    explicit SurfaceShape(QObject *parent = nullptr);
    bool contrastFeedback() const { return m_contrastFeedback; }
    void setContrastFeedback(bool enabled);
    bool contrastOnly() const { return m_contrastOnly; }
    void setContrastOnly(bool enabled);
    qreal backdropLuminance() const { return m_backdropLuminance; }
    QString backdropMapSource() const { return m_backdropMapSource; }
    QString backdropInkMapSource() const { return m_backdropInkMapSource; }
    bool backdropInkUniform() const { return m_backdropInkUniform; }
    bool backdropInkDark() const { return m_backdropInkDark; }
    QSize backdropMapSize() const { return m_backdropMapSize; }
    // Brightness 0..1 of the map cell at normalized (x, y), or -1 without a map.
    Q_INVOKABLE qreal backdropAt(qreal x, qreal y) const;
    bool continuousBlurSupported() const;
    qreal blurSigma() const { return m_blurSigma; }
    int effectType() const { return m_effectType; }
    int material() const { return m_material; }
    int tintMode() const { return m_tintMode; }
    QColor tintColor() const { return m_tintColor; }
    qreal tintOpacity() const { return m_tintOpacity; }
    void setBlurSigma(qreal value);
    void setEffectType(int value);
    void setMaterial(int value);
    void setTintMode(int value);
    void setTintColor(const QColor &value);
    void setTintOpacity(qreal value);
    ~SurfaceShape() override;

    QQuickItem *target() const { return m_target; }
    void setTarget(QQuickItem *target);
    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);
    qreal exponent() const { return m_exponent; }
    void setExponent(qreal exponent);
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool isActive() const { return m_shape != nullptr; }
    QRectF captureGeometry() const { return m_captureGeometry; }
    void setCaptureGeometry(const QRectF &geometry);
    bool fixedCaptureSupported() const;
    qreal materialOpacity() const { return m_materialOpacity; }
    void setMaterialOpacity(qreal opacity);
    bool materialOpacitySupported() const;
    bool revealEnabled() const { return m_revealEnabled; }
    bool revealOpened() const { return m_revealOpened; }
    int revealDuration() const { return m_revealDuration; }
    void setRevealEnabled(bool enabled);
    void setRevealOpened(bool opened);
    void setRevealDuration(int duration);
    bool revealSupported() const;
    bool scrimEnabled() const { return m_scrimEnabled; }
    void setScrimEnabled(bool enabled);
    int scrimTint() const { return m_scrimTint; }
    void setScrimTint(int tint);
    qreal scrimCap() const { return m_scrimCap; }
    void setScrimCap(qreal cap);
    qreal scrimDecay() const { return m_scrimDecay; }
    void setScrimDecay(qreal decay);
    bool blurEnabled() const { return m_blurEnabled; }
    void setBlurEnabled(bool enabled);
    int blurLevel() const { return m_blurLevel; }
    void setBlurLevel(int level);

Q_SIGNALS:
    void finishChanged();
    void backdropLuminanceChanged();
    void backdropMapChanged();
    void targetChanged();
    void radiusChanged();
    void exponentChanged();
    void enabledChanged();
    void activeChanged();
    void captureGeometryChanged();
    void materialOpacityChanged();
    void revealChanged();
    void revealFinished(bool opened);
    void scrimEnabledChanged();
    void scrimTintChanged();
    void scrimCapChanged();
    void scrimDecayChanged();
    void blurEnabledChanged();
    void blurLevelChanged();

private Q_SLOTS:
    void scheduleSync();
    void sync();
    void handleWindowChanged(QQuickWindow *window);
    void rewireAncestors();

private:
    friend class SurfaceShapeSyncItem;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void releaseShape();
    void clearBackdropMap();
    void setBackdropMap(const QSize &size, const QByteArray &cells);
    void updateBackdropInk(bool notify = true);
    void disconnectAncestors();
    wl_surface *nativeSurface() const;

    QPointer<QQuickItem> m_target;
    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_syncItem;
    kos_surface_shape_v1 *m_shape = nullptr;
    wl_surface *m_surface = nullptr;
    bool m_contrastFeedback = false;
    bool m_contrastOnly = false;
    qreal m_backdropLuminance = -1;
    QString m_backdropMapSource;
    QSize m_backdropMapSize;
    quint64 m_backdropMapSerial = 0;
    QByteArray m_backdropMapCells;
    QString m_backdropInkMapSource;
    QByteArray m_backdropInkCells;
    QSize m_backdropInkSize;
    bool m_backdropInkUniform = false;
    bool m_backdropInkDark = false;
    BackdropInkPolicy m_inkPolicy;
    QElapsedTimer m_inkClock;
    QTimer m_inkTimer;
    qreal m_blurSigma = 12;
    int m_effectType = Liquid;
    int m_material = Regular;
    int m_tintMode = NoTint;
    QColor m_tintColor = Qt::black;
    qreal m_tintOpacity = 0;
    qreal m_radius = 0.0;
    QRectF m_captureGeometry;
    qreal m_materialOpacity = 1.0;
    bool m_revealEnabled = false;
    bool m_revealOpened = false;
    int m_revealDuration = 320;
    bool m_revealDirty = true;
    uint32_t m_revealSerial = 0;
    qreal m_exponent = 2.0;
    bool m_enabled = true;
    bool m_scrimEnabled = false;
    int m_scrimTint = 0;
    qreal m_scrimCap = 0.0;
    qreal m_scrimDecay = 1.0;
    bool m_blurEnabled = false;
    int m_blurLevel = 1;
    bool m_syncPending = false;
    // The published geometry is a scene rectangle, so it also moves when an
    // ancestor does -- a change the target's own x/y signals cannot see.
    QList<QMetaObject::Connection> m_ancestorConnections;
};
