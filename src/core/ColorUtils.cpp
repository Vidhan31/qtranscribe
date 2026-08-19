#include "ColorUtils.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>

using namespace Qt::StringLiterals;

ColorUtils::ColorUtils(QObject* parent)
    : QObject(parent) {
    QSettings settings;
    m_themeMode = settings.value(u"ui/themeMode"_s, u"system"_s).toString();

    if (auto* app = qGuiApp) {
        if (auto* hints = app->styleHints()) {
            connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) { updateEffectiveTheme(); });
        }
        app->installEventFilter(this);
    }

    updateEffectiveTheme();
}

bool ColorUtils::eventFilter(QObject* watched, QEvent* event) {
    if (event && event->type() == QEvent::ApplicationPaletteChange) {
        updateEffectiveTheme();
    }
    return QObject::eventFilter(watched, event);
}

QString ColorUtils::themeMode() const {
    return m_themeMode;
}

void ColorUtils::setThemeMode(const QString& mode) {
    if (m_themeMode == mode)
        return;

    m_themeMode = mode;
    QSettings settings;
    settings.setValue(u"ui/themeMode"_s, m_themeMode);

    emit themeModeChanged();
    updateEffectiveTheme();
}

bool ColorUtils::isDark() const {
    return m_isDark;
}

bool ColorUtils::systemThemeIsDark() const {
    return m_systemThemeIsDark;
}

QString ColorUtils::systemThemeName() const {
    return m_systemThemeIsDark ? u"Dark"_s : u"Light"_s;
}

void ColorUtils::updateEffectiveTheme() {
    bool sysDark = true;
    if (auto* hints = QGuiApplication::styleHints()) {
        const auto scheme = hints->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            sysDark = true;
        } else if (scheme == Qt::ColorScheme::Light) {
            sysDark = false;
        } else {
            const QColor winCol = QGuiApplication::palette().color(QPalette::Window);
            sysDark = (winCol.lightnessF() < 0.5);
        }
    } else {
        const QColor winCol = QGuiApplication::palette().color(QPalette::Window);
        sysDark = (winCol.lightnessF() < 0.5);
    }

    const bool sysDarkChanged = (m_systemThemeIsDark != sysDark);
    m_systemThemeIsDark = sysDark;
    if (sysDarkChanged) {
        emit systemThemeChanged();
    }

    bool effectiveDark = true;
    if (m_themeMode == u"dark"_s) {
        effectiveDark = true;
    } else if (m_themeMode == u"light"_s) {
        effectiveDark = false;
    } else {
        effectiveDark = sysDark;
    }

    if (m_isDark != effectiveDark) {
        m_isDark = effectiveDark;
        emit isDarkChanged();
    }
}

QColor ColorUtils::withAlpha(const QColor& baseColor, qreal alpha) {
    QColor c = baseColor;
    c.setAlphaF(static_cast<float>(std::clamp(alpha, 0.0, 1.0)));
    return c;
}

QColor ColorUtils::tint(const QColor& baseColor, const QColor& tintColor) {
    const float a = static_cast<float>(tintColor.alphaF());
    return QColor::fromRgbF(baseColor.redF() * (1.0f - a) + tintColor.redF() * a,
                            baseColor.greenF() * (1.0f - a) + tintColor.greenF() * a,
                            baseColor.blueF() * (1.0f - a) + tintColor.blueF() * a, baseColor.alphaF());
}

QColor ColorUtils::statusBg(const QColor& baseColor, qreal alpha) {
    return withAlpha(baseColor, alpha);
}

QColor ColorUtils::statusBorder(const QColor& baseColor, qreal alpha) {
    return withAlpha(baseColor, alpha);
}
