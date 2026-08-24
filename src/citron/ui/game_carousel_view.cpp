#include <algorithm>
#include <cmath>
#include <QApplication>
#include <QEasingCurve>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QDateTime>
#include <QKeyEvent>
#include <QMovie>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QToolButton>

#include "audio_core/audio_visualizer_tap.h"
#include "citron/ui/game_carousel_view.h"
#include "citron/ui/system_audio_loopback.h"
#include "citron/ui/nexo_profile_chip.h"
#include "citron/ui/nexo_status_cluster.h"
#include "citron/game_list_p.h"
#include "citron/uisettings.h"
#include "citron/theme.h"
#include "citron/custom_metadata.h"
#include "common/nexo_compatible_titles.h"
#include "citron/nexo_ldn_counts.h"
#include "citron/nexo_online_counts.h"
#include "citron/util/image_cache.h"
#include "common/nexo_account.h"

namespace {
constexpr int kBackdropPickerRowH = 40;
constexpr int kBackdropPickerWidth = 168;
const QStringList& BackdropOptionLabels() {
    static const QStringList labels{QObject::tr("Gradient"), QObject::tr("Wave"), QObject::tr("None"),
                                    QObject::tr("Reactive")};
    return labels;
}

QRectF CenterCropForAspect(const QSizeF& source, const QSizeF& dest) {
    if (source.width() <= 0 || source.height() <= 0 || dest.width() <= 0 || dest.height() <= 0) {
        return QRectF(QPointF(0, 0), source);
    }
    const qreal src_aspect = source.width() / source.height();
    const qreal dst_aspect = dest.width() / dest.height();
    if (src_aspect > dst_aspect) {
        const qreal crop_w = source.height() * dst_aspect;
        return QRectF((source.width() - crop_w) / 2.0, 0, crop_w, source.height());
    }
    const qreal crop_h = source.width() / dst_aspect;
    return QRectF(0, (source.height() - crop_h) / 2.0, source.width(), crop_h);
}
}

NeXoBackdropPicker::NeXoBackdropPicker(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    SetScale(1.0);
}

void NeXoBackdropPicker::SetScale(qreal scale) {
    m_scale = std::clamp(scale, 1.0, 1.6);
    const int row_h = static_cast<int>(kBackdropPickerRowH * m_scale);
    setFixedSize(static_cast<int>(kBackdropPickerWidth * m_scale), row_h * BackdropOptionLabels().size() + 8);
}

void NeXoBackdropPicker::PopupAt(const QPoint& global_top_left) {
    move(global_top_left);
    m_hover = -1;
    show();
    // Qt::Popup's implicit "outside click closes it" isn't reliable here — CinematicCarousel
    // does its own mouse handling (drag-scroll, tile clicks) that can eat the event before Qt's
    // popup-grab logic sees it. Watch application-wide instead, and close only for a press that
    // truly lands outside our own rect; a click that came from mousePressEvent (a row, or padding)
    // stays open per that handler's own logic.
    qApp->installEventFilter(this);
}

void NeXoBackdropPicker::hideEvent(QHideEvent* event) {
    qApp->removeEventFilter(this);
    QWidget::hideEvent(event);
}

bool NeXoBackdropPicker::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (!rect().contains(mapFromGlobal(mouse_event->globalPosition().toPoint()))) {
            hide();
        }
    }
    return QWidget::eventFilter(watched, event);
}

int NeXoBackdropPicker::RowAt(const QPoint& pos) const {
    if (pos.x() < 4 || pos.x() >= width() - 4) return -1;
    const int row_h = static_cast<int>(kBackdropPickerRowH * m_scale);
    const int row = (pos.y() - 4) / row_h;
    if (pos.y() < 4 || row < 0 || row >= BackdropOptionLabels().size()) return -1;
    return row;
}

void NeXoBackdropPicker::mouseMoveEvent(QMouseEvent* event) {
    const int row = RowAt(event->pos());
    if (row != m_hover) { m_hover = row; update(); }
}

void NeXoBackdropPicker::leaveEvent(QEvent*) {
    m_hover = -1;
    update();
}

void NeXoBackdropPicker::mousePressEvent(QMouseEvent* event) {
    const int row = RowAt(event->pos());
    if (row >= 0) {
        m_current = row;
        emit ThemeSelected(row);
        update();
        return;
    }
    // Qt::Popup already closes this on any click outside its own geometry; a press that
    // lands inside the popup but not on a row (e.g. the padding) shouldn't dismiss it either.
}

void NeXoBackdropPicker::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath bg;
    bg.addRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);
    p.fillPath(bg, QColor(16, 16, 22, 240));
    p.setPen(QPen(QColor(255, 255, 255, 45), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bg);

    const auto& labels = BackdropOptionLabels();
    const int row_h = static_cast<int>(kBackdropPickerRowH * m_scale);
    QFont f = font();
    f.setPointSizeF(10 * m_scale);
    p.setFont(f);

    const QColor accent(Theme::GetAccentColor());

    for (int i = 0; i < labels.size(); ++i) {
        const QRect row(4, 4 + i * row_h, width() - 8, row_h);
        if (i == m_current) {
            QPainterPath sel;
            sel.addRoundedRect(row, 8, 8);
            QColor sel_fill = accent;
            sel_fill.setAlpha(60);
            p.fillPath(sel, sel_fill);
        } else if (i == m_hover) {
            QPainterPath hov;
            hov.addRoundedRect(row, 8, 8);
            p.fillPath(hov, QColor(255, 255, 255, 18));
        }

        const int dot_x = row.left() + static_cast<int>(16 * m_scale);
        p.setPen(Qt::NoPen);
        p.setBrush(i == m_current ? accent : QColor(255, 255, 255, 70));
        p.drawEllipse(QPointF(dot_x, row.center().y()), 4 * m_scale, 4 * m_scale);

        p.setPen(i == m_current ? Qt::white : QColor(255, 255, 255, 190));
        p.drawText(QRect(dot_x + static_cast<int>(14 * m_scale), row.top(),
                         row.width() - static_cast<int>(30 * m_scale), row.height()),
                   Qt::AlignVCenter | Qt::AlignLeft, labels[i]);
    }
}

CinematicCarousel::CinematicCarousel(QWidget* parent) : QWidget(parent) {
    m_snap_animation = new QPropertyAnimation(this, "focalIndex");
    m_snap_animation->setDuration(350);
    m_snap_animation->setEasingCurve(QEasingCurve::OutCubic);

    m_pulse_timer = new QTimer(this);
    connect(m_pulse_timer, &QTimer::timeout, this, [this]{
        bool needs_update = false;
        m_pulse_tick++;
        needs_update = true; // idle bob animation runs continuously

        // Advance entry animations
        auto it = m_entry_animations.begin();
        while (it != m_entry_animations.end()) {
            if (!it.key().isValid()) {
                it = m_entry_animations.erase(it);
                continue;
            }
            if (it.value() < 1.0) {
                it.value() += 0.06;
                if (it.value() >= 1.0) it.value() = 1.0;
                needs_update = true;
                ++it;
            } else {
                it = m_entry_animations.erase(it);
            }
        }
        if (needs_update) update();
    });
    m_pulse_timer->start(32);

    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    setMinimumHeight(450);
    setContextMenuPolicy(Qt::CustomContextMenu);

    m_momentum_timer = new QTimer(this);
    m_momentum_timer->setInterval(16);
    connect(m_momentum_timer, &QTimer::timeout, this, [this] {
        if (std::abs(m_velocity) < 0.05) {
            m_momentum_timer->stop();
            startSnapAnimation(std::round(m_focal_index));
            return;
        }

        setFocalIndex(m_focal_index + m_velocity);
        m_velocity *= 0.92; // Friction factor
    });

    m_profile_chip = new NeXoProfileChip(this);
    connect(m_profile_chip, &NeXoProfileChip::Clicked, this, &CinematicCarousel::ProfileClicked);
    m_status_cluster = new NeXoStatusCluster(this);

    m_backdrop_btn = new QToolButton(this);
    m_backdrop_btn->setText(QStringLiteral("◐"));
    m_backdrop_btn->setToolTip(tr("Backdrop Theme"));
    m_backdrop_btn->setCursor(Qt::PointingHandCursor);
    m_backdrop_btn->setAutoRaise(true);
    m_backdrop_btn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  border: 1px solid rgba(255, 255, 255, 60);"
        "  border-radius: 18px;"
        "  background: rgba(255, 255, 255, 20);"
        "  color: white;"
        "  font-size: 15px;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(255, 255, 255, 45);"
        "  border-color: rgba(255, 255, 255, 120);"
        "}"));
    m_backdrop_picker = new NeXoBackdropPicker(this);
    connect(m_backdrop_picker, &NeXoBackdropPicker::ThemeSelected, this, [this](int index) {
        const auto theme = static_cast<BackdropTheme>(index);
        SetBackdropTheme(theme);
        emit BackdropThemeChanged(index);
    });
    connect(m_backdrop_btn, &QToolButton::clicked, this, [this] {
        m_backdrop_picker->SetCurrent(static_cast<int>(m_backdrop_theme));
        m_backdrop_picker->PopupAt(m_backdrop_btn->mapToGlobal(QPoint(0, m_backdrop_btn->height() + 6)));
    });

    m_profile_chip->raise();
    m_backdrop_btn->raise();
    m_status_cluster->raise();
}

QModelIndex CinematicCarousel::currentIndex() const {
    if (!m_model || m_model->rowCount() == 0) return QModelIndex();
    return m_model->index(std::round(m_focal_index), 0);
}

void CinematicCarousel::SetBackdropTheme(BackdropTheme theme) {
    m_backdrop_theme = theme;
    if (theme == BackdropTheme::Reactive) {
        SystemAudioLoopback::Start();
    } else {
        SystemAudioLoopback::Stop();
    }
    update();
}

void CinematicCarousel::SetBackdropImage(const QString& path, u8 opacity) {
    m_backdrop_image_opacity = opacity;
    if (path == m_backdrop_image_path) {
        update();
        return;
    }
    m_backdrop_image_path = path;
    if (m_backdrop_movie) {
        m_backdrop_movie->stop();
        delete m_backdrop_movie;
        m_backdrop_movie = nullptr;
    }
    if (!path.isEmpty()) {
        m_backdrop_movie = new QMovie(path, QByteArray(), this);
        m_backdrop_movie->setCacheMode(QMovie::CacheAll);
        connect(m_backdrop_movie, &QMovie::frameChanged, this, [this](int) { update(); });
        m_backdrop_movie->start();
    }
    update();
}

qreal CinematicCarousel::HeroSize() const {
    const int is = UISettings::values.game_icon_size.GetValue();
    return is + 60.0;
}

qreal CinematicCarousel::Stride() const {
    return HeroSize() + std::clamp(width() * 0.012, 10.0, 22.0);
}

// The NeXium carousel_view() layout formula: cards fan out from the focal item with distance-based
// scale/vertical arc, plus a slow idle bob so the whole row feels alive rather than static.
QRectF CinematicCarousel::CardGeometry(int index, bool with_bob) const {
    const qreal hero_size = HeroSize();
    const qreal stride = Stride();
    const qreal cx0 = width() * 0.32; // off-center like NeXium, so the next games peek in on the right
    const qreal cy0 = height() / 2.0;

    const qreal diff = index - m_focal_index;
    const qreal abs_diff = std::abs(diff);
    const qreal scale = std::max(1.0 - abs_diff * 0.05, 0.82);
    const qreal sz = hero_size * scale;
    const qreal card_h = UISettings::values.game_list_poster_view.GetValue() ? sz * 1.5 : sz;
    const qreal cx = cx0 + diff * stride;
    qreal cy = cy0 + abs_diff * 6.0;
    if (with_bob) {
        const qreal t = m_pulse_tick * 0.032;
        cy += std::sin(t * 1.25 + index * 0.9) * 2.6 * scale;
    }
    return QRectF(cx - sz / 2.0, cy - card_h / 2.0, sz, card_h);
}

QModelIndex CinematicCarousel::indexAt(const QPoint& point) const {
    if (!m_model) return QModelIndex();
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (CardGeometry(i, false).contains(point)) return m_model->index(i, 0);
    }
    return QModelIndex();
}

QRect CinematicCarousel::visualRect(const QModelIndex& index) const {
    if (!m_model || !index.isValid()) return QRect();
    return CardGeometry(index.row(), false).toRect();
}

void CinematicCarousel::setModel(QAbstractItemModel* model) {
    if (m_model) {
        m_model->disconnect(this);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]() { update(); });
        connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { update(); });
        connect(m_model, &QAbstractItemModel::dataChanged, this, [this]() { update(); });
    }
    if (m_model && m_model->rowCount() > 0) setFocalIndex(0.0);
    update();
}
void CinematicCarousel::setFocalIndex(qreal index) {
    if (!m_model || m_model->rowCount() == 0) m_focal_index = 0.0;
    else m_focal_index = std::max(0.0, std::min(static_cast<qreal>(m_model->rowCount() - 1), index));
    updateFocalItem(); update();
}

void CinematicCarousel::scrollTo(int index) { if (!m_model || index < 0 || index >= m_model->rowCount()) return; startSnapAnimation(index); }

void CinematicCarousel::RegisterEntryAnimation(const QModelIndex& index) {
    if (index.isValid()) {
        m_entry_animations[QPersistentModelIndex(index)] = 0.0;
    }
}

void CinematicCarousel::scrollToLetter(QChar letter) {
    if (!m_model) return;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QString title = m_model->index(i, 0).data(Qt::DisplayRole).toString();
        if (!title.isEmpty() && title[0].toUpper() == letter.toUpper()) { scrollTo(i); return; }
    }
}

void CinematicCarousel::ApplyTheme() {
    update();
}

void CinematicCarousel::setControllerFocus(bool focus) { m_has_focus = focus; update(); }

void CinematicCarousel::onNavigated(int dx, int dy) { if (!m_has_focus || !m_model || m_model->rowCount() == 0) return; startSnapAnimation(std::round(m_focal_index + dx)); }

void CinematicCarousel::onActivated() { if (!m_has_focus) return; QModelIndex idx = currentIndex(); if (idx.isValid()) emit itemActivated(idx); }

void CinematicCarousel::onCancelled() {}

void CinematicCarousel::DrawBackdrop(QPainter& p, const QRectF& bg_rect) const {
    const bool has_image = m_backdrop_movie && m_backdrop_movie->isValid();
    if (!has_image) {
        p.fillRect(bg_rect, CardBg());
    }

    if (m_backdrop_theme == BackdropTheme::None) {
        return;
    }

    const QColor acc = AccentColor();
    if (m_backdrop_theme == BackdropTheme::Gradient) {
        const qreal top_y = bg_rect.top() + bg_rect.height() * 0.42;
        const qreal bot_y = bg_rect.bottom();
        constexpr int kCols = 96;
        const qreal col_w = bg_rect.width() / kCols;
        for (int c = 0; c < kCols; ++c) {
            const qreal nx = (c + 0.5) / kCols;
            const qreal x = bg_rect.left() + nx * bg_rect.width();
            const qreal d = std::abs(nx - 0.5) * 2.0;
            const qreal horiz = std::max(1.0 - d * d, 0.0);
            const int bottom_alpha = static_cast<int>(std::clamp(225.0 * horiz, 0.0, 255.0));

            QLinearGradient col_grad(QPointF(x, top_y), QPointF(x, bot_y));
            QColor top_col = acc; top_col.setAlpha(0);
            QColor mid_col = acc; mid_col.setAlpha(static_cast<int>(bottom_alpha * 0.22));
            QColor bot_col = acc; bot_col.setAlpha(bottom_alpha);
            col_grad.setColorAt(0.0, top_col);
            col_grad.setColorAt(0.45, mid_col);
            col_grad.setColorAt(1.0, bot_col);
            p.fillRect(QRectF(x - col_w / 2.0 - 0.5, top_y, col_w + 1.0, bot_y - top_y), col_grad);
        }
        return;
    }

    if (m_backdrop_theme == BackdropTheme::Reactive) {
        const auto bands = AudioCore::AudioVisualizerTap::GetBands();
        const qreal bass = bands.bass;
        const qreal mid = bands.mid;
        const qreal treble = bands.treble;
        const qreal t = m_pulse_tick * 0.032;
        const qreal core_hue = 355.0 - treble * 140.0;

        const auto hsl = [](qreal h, qreal s, qreal l, qreal a) {
            qreal hf = std::fmod(h, 360.0);
            if (hf < 0.0) hf += 360.0;
            return QColor::fromHslF(hf / 360.0, std::clamp(s, 0.0, 1.0), std::clamp(l, 0.0, 1.0),
                                    std::clamp(a, 0.0, 1.0));
        };

        // Corner ink blooms, sharing the ring's hue (small per-corner offset only).
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        const QPointF corners[4] = {bg_rect.topLeft(), bg_rect.topRight(), bg_rect.bottomLeft(),
                                    bg_rect.bottomRight()};
        const qreal reach = std::min(bg_rect.width(), bg_rect.height()) * (0.30 + bass * 0.22);
        for (int i = 0; i < 4; ++i) {
            const qreal breathe = 0.55 + 0.45 * std::sin(t * 1.1 + i * 1.7);
            const qreal r = std::max(2.0, reach * (0.7 + bass * 0.5) * breathe);
            const qreal h = core_hue + (i - 1.5) * 8.0;
            QRadialGradient grad(corners[i], r);
            grad.setColorAt(0.0, hsl(h, 0.82, 0.56, 0.20 + bass * 0.20));
            grad.setColorAt(1.0, hsl(h, 0.82, 0.56, 0.0));
            p.setBrush(grad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(corners[i], r, r);
        }
        p.restore();

        // Concentric shock rings, pushed outward by bass with mid-driven wobble.
        const QPointF center = bg_rect.center();
        for (int i = 0; i < 5; ++i) {
            const qreal wob = std::sin(t * 2.0 + i * 1.3) * 10.0 * (0.3 + mid);
            const qreal r = std::max(2.0, 26.0 + i * (36.0 + bass * 60.0) + wob);
            const qreal h = core_hue - i * 6.0;
            p.setPen(QPen(hsl(h, 0.78, 0.52 + treble * 0.18, std::max(0.08, 0.55 - i * 0.09)),
                         2.0 + bass * 7.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(center, r, r);
        }

        const qreal core_r = 10.0 + bass * 34.0;
        QRadialGradient core_grad(center, core_r * 2.2);
        core_grad.setColorAt(0.0, hsl(core_hue, 0.90, 0.62, 1.0));
        core_grad.setColorAt(1.0, hsl(core_hue, 0.90, 0.62, 0.0));
        p.setBrush(core_grad);
        p.setPen(Qt::NoPen);
        p.drawEllipse(center, core_r * 2.2, core_r * 2.2);
        return;
    }

    QColor wash = acc;
    wash.setAlpha(14);
    p.fillRect(bg_rect, wash);

    struct WaveBand { qreal base_frac, amp_frac, phase_off, speed, shade; int alpha; };
    static constexpr WaveBand kBands[] = {
        {0.63, 0.043, 0.0, 1.5, 0.25, 49},
        {0.54, 0.060, 1.1, 1.2, -0.30, 41},
        {0.72, 0.030, 2.3, 0.9, 0.45, 34},
        {0.45, 0.077, 0.6, 1.7, -0.40, 28},
        {0.35, 0.038, 4.0, 0.6, 0.55, 18},
    };
    const qreal t = m_pulse_tick * 0.032;
    const auto shade = [](QColor c, qreal f) {
        const auto adj = [f](int v) {
            return f >= 0.0 ? static_cast<int>(v + (255 - v) * f) : static_cast<int>(v * (1.0 + f));
        };
        return QColor(adj(c.red()), adj(c.green()), adj(c.blue()));
    };
    constexpr int kSteps = 48;
    for (const auto& band : kBands) {
        const qreal base_y = bg_rect.top() + bg_rect.height() * band.base_frac;
        const qreal amp = bg_rect.height() * band.amp_frac;
        const qreal ph = t * band.speed + band.phase_off;

        QPainterPath fill;
        fill.moveTo(bg_rect.left(), bg_rect.bottom());
        for (int s = 0; s <= kSteps; ++s) {
            const qreal nx = static_cast<qreal>(s) / kSteps;
            const qreal x = bg_rect.left() + nx * bg_rect.width();
            const qreal y = base_y + std::sin(nx * 2 * M_PI + ph) * amp +
                           std::cos(nx * 2 * M_PI * 1.7 + ph * 0.8) * amp * 0.4;
            fill.lineTo(x, y);
        }
        fill.lineTo(bg_rect.right(), bg_rect.bottom());
        fill.closeSubpath();

        QColor band_color = shade(acc, band.shade);
        band_color.setAlpha(band.alpha);
        p.fillPath(fill, band_color);
    }
}

void CinematicCarousel::RefreshBackdropCache(const QSize& logical_size) {
    if (logical_size.isEmpty()) return;
    constexpr qreal kMaxDim = 420.0;
    const qreal scale = std::min(1.0, kMaxDim / std::max(logical_size.width(), logical_size.height()));
    QSize render_size(std::max(32, static_cast<int>(logical_size.width() * scale)),
                      std::max(32, static_cast<int>(logical_size.height() * scale)));

    m_backdrop_cache = QPixmap(render_size);
    m_backdrop_cache.fill(Qt::transparent);
    QPainter cp(&m_backdrop_cache);
    cp.setRenderHint(QPainter::Antialiasing);
    DrawBackdrop(cp, QRectF(QPointF(0, 0), render_size));
    m_backdrop_cache_logical_size = logical_size;
    m_backdrop_cache_tick = m_pulse_tick;
    m_backdrop_cache_accent = AccentColor();
    m_backdrop_cache_theme = m_backdrop_theme;
    m_backdrop_cache_image_path = m_backdrop_image_path;
    m_backdrop_cache_image_opacity = m_backdrop_image_opacity;
}

void CinematicCarousel::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);

    const QRectF bg_rect = rect();
    const QColor acc = AccentColor();

    if (m_backdrop_movie && m_backdrop_movie->isValid()) {
        const QPixmap frame = m_backdrop_movie->currentPixmap();
        if (!frame.isNull()) {
            p.fillRect(bg_rect, CardBg());
            p.save();
            p.setOpacity(m_backdrop_image_opacity / 255.0);
            p.drawPixmap(bg_rect, frame, CenterCropForAspect(frame.size(), bg_rect.size()));
            p.restore();
        }
    }

    const bool animated = m_backdrop_theme == BackdropTheme::Wave ||
                          m_backdrop_theme == BackdropTheme::Reactive ||
                          (m_backdrop_movie && m_backdrop_movie->isValid() &&
                           m_backdrop_movie->frameCount() != 1);
    const qint64 stale_after = animated ? 1 : 1000000;
    if (m_backdrop_cache_logical_size != bg_rect.size().toSize() || m_backdrop_cache_accent != acc ||
        m_backdrop_cache_theme != m_backdrop_theme ||
        m_backdrop_cache_image_path != m_backdrop_image_path ||
        m_backdrop_cache_image_opacity != m_backdrop_image_opacity ||
        m_pulse_tick - m_backdrop_cache_tick >= stale_after) {
        RefreshBackdropCache(bg_rect.size().toSize());
    }
    p.drawPixmap(bg_rect, m_backdrop_cache, m_backdrop_cache.rect());

    if (!m_model || m_model->rowCount() == 0) return;

    const int count = m_model->rowCount();
    const qreal stride = Stride();
    const qreal cull_max = std::max(((width() / 2.0) / stride) + 1.6, 4.6);
    const qreal fade_start = cull_max - 1.6;

    const int focal_idx = std::round(m_focal_index);
    const int range = std::max(5, static_cast<int>(cull_max) + 1);
    const int start_idx = std::max(0, focal_idx - range);
    const int end_idx = std::min(count - 1, focal_idx + range);

    QVector<int> order;
    for (int i = start_idx; i <= end_idx; ++i) order << i;
    std::sort(order.begin(), order.end(), [this](int a, int b) { return std::abs(a - m_focal_index) > std::abs(b - m_focal_index); });

    for (int i : order) {
        const qreal diff = i - m_focal_index;
        const qreal abs_diff = std::abs(diff);
        if (abs_diff > cull_max) continue;

        qreal alpha = std::clamp(1.0 - abs_diff * 0.03, 0.0, 1.0);
        if (abs_diff > fade_start) {
            alpha *= std::clamp((cull_max - abs_diff) / (cull_max - fade_start), 0.0, 1.0);
        }

        qreal entry_anim = 1.0;
        QPersistentModelIndex pidx(m_model->index(i, 0));
        if (m_entry_animations.contains(pidx)) entry_anim = m_entry_animations[pidx];
        alpha *= entry_anim;
        if (alpha <= 0.005) continue;

        const bool focal = (i == focal_idx);
        QRectF card = CardGeometry(i, !focal); // freeze the selected card so its ring doesn't jitter
        if (entry_anim < 1.0) {
            const qreal pop_s = 0.7 + entry_anim * 0.3;
            const QPointF c = card.center();
            card = QRectF(0, 0, card.width() * pop_s, card.height() * pop_s);
            card.moveCenter(c);
        }

        p.save();
        QPainterPath shadow;
        shadow.addRoundedRect(card.translated(0, 6), 14, 14);
        p.fillPath(shadow, QColor(0, 0, 0, static_cast<int>(120 * alpha)));

        if (focal) {
            QPainterPath outer;
            outer.addRoundedRect(card.adjusted(-8, -8, 8, 8), 16, 16);
            QColor outer_col = acc;
            outer_col.setAlphaF(static_cast<float>((m_has_focus ? 0.18 : 0.16) * alpha));
            p.fillPath(outer, outer_col);

            QPainterPath ring;
            ring.addRoundedRect(card.adjusted(-3, -3, 3, 3), 14, 14);
            QColor ring_col = acc;
            ring_col.setAlphaF(static_cast<float>((m_has_focus ? 0.85 : 0.4) * alpha));
            p.setPen(QPen(ring_col, m_has_focus ? 5.0 : 3.0));
            p.setBrush(Qt::NoBrush);
            p.drawPath(ring);
        }

        p.setOpacity(alpha);

        QModelIndex idx = m_model->index(i, 0);
        u64 program_id = idx.data(GameListItemPath::ProgramIdRole).toULongLong();
        QPixmap pix;
        if (UISettings::values.game_list_poster_view.GetValue()) {
            pix = Citron::ImageCache::GetCustomPoster(program_id);
        }
        if (pix.isNull()) pix = Citron::ImageCache::GetCustomIcon(program_id);
        if (pix.isNull()) pix = idx.data(GameListItemPath::HighResIconRole).value<QPixmap>();
        if (pix.isNull()) pix = idx.data(Qt::DecorationRole).value<QPixmap>();
        if (!pix.isNull()) {
            QPainterPath clip;
            clip.addRoundedRect(card, 14, 14);
            p.setClipPath(clip);
            p.drawPixmap(card, pix, CenterCropForAspect(pix.size(), card.size()));
            p.setClipping(false);
            DrawOnlineBadges(p, card, program_id, idx.data(GameListItemPath::VersionRole).toString());
        } else {
            QPainterPath clip;
            clip.addRoundedRect(card, 14, 14);
            p.setClipPath(clip);
            p.fillRect(card, CardBg().lighter(130));
            p.setClipping(false);
        }
        p.restore();

        if (focal) {
            p.save();
            p.setOpacity(alpha);
            QFont title_font = font();
            title_font.setBold(true);
            title_font.setPointSizeF(title_font.pointSizeF() + 4);
            p.setFont(title_font);
            p.setPen(TextColor());
            const QRectF title_rect(card.left() - 80, card.bottom() + 12, card.width() + 160, 36);
            const QString title = idx.data(Qt::DisplayRole).toString();
            p.drawText(title_rect, Qt::AlignHCenter | Qt::AlignTop,
                       QFontMetrics(title_font).elidedText(title, Qt::ElideRight,
                                                            static_cast<int>(title_rect.width())));
            p.restore();
        }
    }
}

void CinematicCarousel::mousePressEvent(QMouseEvent* event) {
    if (m_snap_animation->state() == QAbstractAnimation::Running) m_snap_animation->stop();
    if (m_momentum_timer->isActive()) m_momentum_timer->stop();

    if (event->button() == Qt::LeftButton) {
        m_last_mouse_pos = event->pos(); m_drag_start_pos = event->pos(); m_is_dragging = true;
        m_velocity = 0.0;
        m_last_move_timestamp = QDateTime::currentMSecsSinceEpoch();
    }
}

void CinematicCarousel::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pt = event->pos();
    if (!m_is_dragging) return;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 dt = now - m_last_move_timestamp;

    qreal dx = m_last_mouse_pos.x() - pt.x();
    qreal delta_index = dx / Stride();

    if (dt > 0) {
        m_velocity = (delta_index / static_cast<qreal>(dt)) * 16.0;
    }

    setFocalIndex(m_focal_index + delta_index);
    m_last_mouse_pos = pt;
    m_last_move_timestamp = now;
}

void CinematicCarousel::mouseReleaseEvent(QMouseEvent* event) {
    m_is_dragging = false;

    // Handle standard click
    if ((event->pos() - m_drag_start_pos).manhattanLength() < 15) {
        QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) { startSnapAnimation(idx.row()); return; }
    }

    // Begin momentum glide if velocity is significant
    if (std::abs(m_velocity) > 0.05) {
        m_momentum_timer->start();
    } else {
        startSnapAnimation(std::round(m_focal_index));
    }
}

void CinematicCarousel::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) { QModelIndex idx = iconAt(event->pos()); if (idx.isValid()) emit itemActivated(idx); }
}

QModelIndex CinematicCarousel::iconAt(const QPoint& pt) const {
    return indexAt(pt);
}

void CinematicCarousel::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_X || event->key() == Qt::Key_Y) {
        QModelIndex cur = currentIndex();
        if (cur.isValid()) {
            int type = cur.data(GameListItem::TypeRole).toInt();
            QString title = cur.data(Qt::DisplayRole).toString();
            QChar cc = title.isEmpty() ? QLatin1Char(' ') : title[0].toUpper();
            int tot = m_model->rowCount(); int sr = cur.row();
            for (int i = 1; i <= tot; ++i) {
                int nr = (sr + i) % tot;
                QModelIndex nidx = m_model->index(nr, 0);
                int nt = nidx.data(GameListItem::TypeRole).toInt();
                if (nt != type) { scrollTo(nr); return; } // Section jump

                if (nt != static_cast<int>(GameListItemType::Favorites)) {
                    QString ntit = nidx.data(Qt::DisplayRole).toString();
                    QChar nc = ntit.isEmpty() ? QLatin1Char(' ') : ntit[0].toUpper();
                    if (nc != cc) { scrollTo(nr); return; } // Alpha jump
                }
            }
        }
    }
    QWidget::keyPressEvent(event);
}

void CinematicCarousel::wheelEvent(QWheelEvent* event) { const int d = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y(); setFocalIndex(m_focal_index - (d / 120.0)); startSnapAnimation(std::round(m_focal_index)); }

void CinematicCarousel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const qreal top_s = std::clamp(height() / 820.0, 1.0, 2.4);
    if (m_profile_chip) {
        m_profile_chip->SetScale(top_s);
        m_profile_chip->move(static_cast<int>(18 * top_s), static_cast<int>(14 * top_s));
    }
    if (m_backdrop_btn) {
        const int d = static_cast<int>(36 * top_s);
        m_backdrop_btn->setFixedSize(d, d);
        QFont f = m_backdrop_btn->font();
        f.setPointSizeF(11 * top_s);
        m_backdrop_btn->setFont(f);
        const int x = m_profile_chip ? m_profile_chip->x() + m_profile_chip->width() + static_cast<int>(10 * top_s)
                                     : static_cast<int>(18 * top_s);
        const int y = m_profile_chip ? m_profile_chip->y() + (m_profile_chip->height() - d) / 2
                                     : static_cast<int>(14 * top_s);
        m_backdrop_btn->move(x, y);
    }
    if (m_backdrop_picker) {
        m_backdrop_picker->SetScale(top_s);
    }
    if (m_status_cluster) {
        m_status_cluster->SetScale(top_s);
        m_status_cluster->move(width() - m_status_cluster->width() - static_cast<int>(18 * top_s),
                               static_cast<int>(14 * top_s));
    }
    update();
}

void CinematicCarousel::startSnapAnimation(qreal target) { m_snap_animation->stop(); m_snap_animation->setStartValue(m_focal_index); m_snap_animation->setEndValue(target); m_snap_animation->start(); }

void CinematicCarousel::updateFocalItem() { if (!m_model) return; int idx = std::round(m_focal_index); if (idx >= 0 && idx < m_model->rowCount()) emit focalItemChanged(m_model->index(idx, 0)); }

void CinematicCarousel::focusOutEvent(QFocusEvent* event) { m_is_dragging = false; update(); QWidget::focusOutEvent(event); }
void CinematicCarousel::leaveEvent(QEvent* event) { m_is_dragging = false; update(); QWidget::leaveEvent(event); }


QColor CinematicCarousel::CardBg() const {
    return Theme::IsDarkMode() ? QColor(18, 20, 26) : QColor(240, 240, 245);
}
QColor CinematicCarousel::TextColor() const {
    return Theme::IsDarkMode() ? QColor(255, 255, 255) : QColor(45, 45, 48);
}
QColor CinematicCarousel::AccentColor() const {
    const QString h = QString::fromStdString(UISettings::values.accent_color.GetValue());
    QColor acc = QColor(h).isValid() ? QColor(h) : QColor(0, 150, 255);
    if (!Theme::IsDarkMode() && acc.lightnessF() > 0.6) {
        acc.setHslF(acc.hslHueF(), acc.hslSaturationF(), 0.5);
    } else if (Theme::IsDarkMode() && acc.lightnessF() < 0.4) {
        acc.setHslF(acc.hslHueF(), acc.hslSaturationF(), 0.6);
    }
    return acc;
}

void CinematicCarousel::DrawOnlineBadges(QPainter& p, const QRectF& card, u64 program_id,
                                         const QString& installed_version) const {
    const bool show_nexo = Common::NeXoAccount::IsLinked() &&
                               NeXo::CompatibleTitles::Table().contains(program_id);
    const std::optional<NeXo::LdnCounts::Stats> ldn = NeXo::LdnCounts::For(program_id);
    if (!show_nexo && !ldn) {
        return;
    }
    const bool needs_update = show_nexo && !NeXo::CompatibleTitles::IsVersionOk(
                                                    program_id, installed_version.toStdString());

    const qreal scale = std::clamp(card.width() / 130.0, 0.55, 1.5);
    const qreal dot_d = 8.0 * scale;
    const qreal pad = 7.0 * scale;
    const qreal badge_h = 18.0 * scale;
    const qreal row_gap = 4.0 * scale;

    QFont f = font();
    f.setBold(true);
    f.setPixelSize(std::max(9, static_cast<int>(10 * scale)));
    const QFontMetrics fm(f);

    p.save();
    p.setRenderHint(QPainter::Antialiasing);

    // row stacks badges sharing the same corner (e.g. player count above "Requires X.X.X").
    auto draw_corner = [&](bool top_right, int row, const QString& text, const QColor& color) {
        const qreal text_w = fm.horizontalAdvance(text);
        const qreal badge_w = dot_d + 6.0 * scale + text_w + 8.0 * scale;
        const qreal y = card.top() + pad + row * (badge_h + row_gap);
        const QRectF badge_rect = top_right ? QRectF(card.right() - badge_w - pad, y, badge_w, badge_h)
                                            : QRectF(card.left() + pad, y, badge_w, badge_h);

        QPainterPath bg;
        bg.addRoundedRect(badge_rect, badge_h / 2.0, badge_h / 2.0);
        p.fillPath(bg, QColor(12, 12, 16, 195));

        const QRectF dot(badge_rect.left() + 5.0 * scale, badge_rect.center().y() - dot_d / 2.0,
                         dot_d, dot_d);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(dot);

        p.setFont(f);
        p.setPen(Qt::white);
        const QRectF text_rect(dot.right() + 4.0 * scale, badge_rect.top(),
                               text_w + 4.0 * scale, badge_h);
        p.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, text);
    };

    if (show_nexo) {
        const int players = NeXo::OnlineCounts::For(program_id);
        draw_corner(true, 0, QString::number(players), QColor(50, 195, 85));
        if (needs_update) {
            const QString required_version =
                QString::fromStdString(NeXo::CompatibleTitles::Table().at(program_id));
            draw_corner(true, 1, tr("Requires %1").arg(required_version), QColor(0, 190, 255));
        }
    }
    if (ldn) {
        draw_corner(false, 0, QString::number(ldn->players), QColor(0, 170, 255));
    }

    p.restore();
}

GameCarouselView::GameCarouselView(QWidget* parent) : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_carousel = new CinematicCarousel(this);
    m_carousel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addWidget(m_carousel, 1);

    connect(m_carousel, &CinematicCarousel::focalItemChanged, this, &GameCarouselView::itemSelectionChanged);
    connect(m_carousel, &CinematicCarousel::itemActivated, this, &GameCarouselView::itemActivated);
    connect(m_carousel, &CinematicCarousel::ProfileClicked, this, &GameCarouselView::ProfileClicked);
    connect(m_carousel, &CinematicCarousel::BackdropThemeChanged, this, &GameCarouselView::BackdropThemeChanged);
    ApplyTheme();
}

void GameCarouselView::ApplyTheme() { m_carousel->ApplyTheme(); }

void GameCarouselView::setModel(QAbstractItemModel* model) { m_carousel->setModel(model); }
