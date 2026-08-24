#pragma once

#include <QAbstractItemModel>
#include <QPropertyAnimation>
#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QHash>

#include "common/common_types.h"

class NeXoProfileChip;
class NeXoStatusCluster;
class QLabel;
class QToolButton;
class QMovie;

class NeXoBackdropPicker : public QWidget {
    Q_OBJECT

public:
    explicit NeXoBackdropPicker(QWidget* parent = nullptr);

    void SetScale(qreal scale);
    void SetCurrent(int index) { m_current = index; update(); }
    void PopupAt(const QPoint& global_top_left);

signals:
    void ThemeSelected(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int RowAt(const QPoint& pos) const;

    int m_current = 0;
    int m_hover = -1;
    qreal m_scale = 1.0;
};

class CinematicCarousel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal focalIndex READ focalIndex WRITE setFocalIndex)

public:
    enum class BackdropTheme { Gradient, Wave, None, Reactive };

    explicit CinematicCarousel(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model);
    void SetBackdropTheme(BackdropTheme theme);
    void SetBackdropImage(const QString& path, u8 opacity);
    qreal focalIndex() const { return m_focal_index; }
    void setFocalIndex(qreal index);

    QModelIndex currentIndex() const;
    QModelIndex indexAt(const QPoint& point) const;
    QRect visualRect(const QModelIndex& index) const;
    QWidget* viewport() const { return const_cast<CinematicCarousel*>(this); }
    QAbstractItemModel* model() const { return m_model; }

    void scrollTo(int index);
    void scrollToLetter(QChar letter);
    void RegisterEntryAnimation(const QModelIndex& index);
    void ApplyTheme();

    void setControllerFocus(bool focus);
    bool hasControllerFocus() const { return m_has_focus; }

public slots:
    void onNavigated(int dx, int dy);
    void onActivated();
    void onCancelled();

signals:
    void focalItemChanged(const QModelIndex& index);
    void itemActivated(const QModelIndex& index);
    void ProfileClicked();
    void BackdropThemeChanged(int theme);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void startSnapAnimation(qreal target);
    void updateFocalItem();
    QModelIndex iconAt(const QPoint& point) const;
    qreal HeroSize() const;
    qreal Stride() const;
    QRectF CardGeometry(int index, bool with_bob) const;
    void DrawBackdrop(QPainter& p, const QRectF& bg_rect) const;
    void RefreshBackdropCache(const QSize& logical_size);
    void DrawOnlineBadges(QPainter& p, const QRectF& card, u64 program_id,
                          const QString& installed_version) const;

    BackdropTheme m_backdrop_theme = BackdropTheme::Gradient;
    QPixmap m_backdrop_cache;
    QSize m_backdrop_cache_logical_size;
    qint64 m_backdrop_cache_tick = -1000;
    QColor m_backdrop_cache_accent;
    BackdropTheme m_backdrop_cache_theme = BackdropTheme::None;
    QString m_backdrop_cache_image_path;
    u8 m_backdrop_cache_image_opacity = 0;
    QString m_backdrop_image_path;
    u8 m_backdrop_image_opacity = 200;
    QMovie* m_backdrop_movie = nullptr;
    QAbstractItemModel* m_model = nullptr;
    qreal m_focal_index = 0.0;
    QPropertyAnimation* m_snap_animation = nullptr;

    QTimer* m_pulse_timer = nullptr;
    qint64 m_pulse_tick = 0;

    QPoint m_last_mouse_pos;
    QPoint m_drag_start_pos;
    bool m_is_dragging = false;

    QColor CardBg() const;
    QColor TextColor() const;
    QColor AccentColor() const;

    bool m_has_focus = false;
    NeXoProfileChip* m_profile_chip = nullptr;
    NeXoStatusCluster* m_status_cluster = nullptr;
    QToolButton* m_backdrop_btn = nullptr;
    NeXoBackdropPicker* m_backdrop_picker = nullptr;

    // Momentum / Physics members
    QTimer* m_momentum_timer = nullptr;
    qreal m_velocity = 0.0;
    qint64 m_last_move_timestamp = 0;

    mutable QMap<QPersistentModelIndex, qreal> m_entry_animations;
};

class GameCarouselView : public QWidget {
    Q_OBJECT

public:
    explicit GameCarouselView(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model);
    void ApplyTheme();
    void SetBackdropTheme(CinematicCarousel::BackdropTheme theme) {
        m_carousel->SetBackdropTheme(theme);
    }
    void SetBackdropImage(const QString& path, u8 opacity) {
        m_carousel->SetBackdropImage(path, opacity);
    }

    CinematicCarousel* view() const { return m_carousel; }
    QAbstractItemModel* model() const { return m_carousel->model(); }

    void setControllerFocus(bool focus) { m_carousel->setControllerFocus(focus); }
    bool hasControllerFocus() const { return m_carousel->hasControllerFocus(); }

public slots:
    void onNavigated(int dx, int dy) { m_carousel->onNavigated(dx, dy); }
    void onActivated() { m_carousel->onActivated(); }
    void onCancelled() { m_carousel->onCancelled(); }

signals:
    void itemActivated(const QModelIndex& index);
    void itemSelectionChanged(const QModelIndex& index);
    void focusReturned();
    void ProfileClicked();
    void BackdropThemeChanged(int theme);

private:
    CinematicCarousel* m_carousel = nullptr;
    QVBoxLayout* m_layout = nullptr;
};
