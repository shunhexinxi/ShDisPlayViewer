#include "EnvironmentSettingsDialog.h"
#include "ui/AppTheme.h"
#include "ui_EnvironmentSettingPage.h"
#include "ui_EnvironmentLightPage.h"
#include "ui_EnvironmentRainPage.h"
#include "ui_EnvironmentSnowPage.h"
#include "ui_EnvironmentFogPage.h"
#include "ui_EnvironmentTimePage.h"
#include <QSignalBlocker>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace Zqxxxt {
namespace {
QDateTime timeOf(const EnvironmentSettings &value) {
    QDate date=QDate::fromString(value.calendarDate,Qt::ISODate);
    return QDateTime(date.isValid()?date:QDate::currentDate(),
        QTime(qBound(0,value.timeOfDay,23),qBound(0,value.minuteOfHour,59),qBound(0,value.secondOfMinute,59)));
}
void putTime(EnvironmentSettings &value,const QDateTime &time) {
    value.calendarDate=time.date().toString(Qt::ISODate);
    value.timeOfDay=time.time().hour(); value.minuteOfHour=time.time().minute();
    value.secondOfMinute=time.time().second();
}
}
class EnvironmentSettingsDialog::Impl {
public:
    Ui::EnvironmentSettingPage shell;
    Ui::EnvironmentLightPage light;
    Ui::EnvironmentRainPage rain;
    Ui::EnvironmentSnowPage snow;
    Ui::EnvironmentFogPage fog;
    Ui::EnvironmentTimePage time;
    EnvironmentSettings original;
    Preview onPreview; Locate onLocate; TimeProvider clock;
    QDateTime requestedTime;
    bool populating=false, running=false;
};

EnvironmentSettingsDialog::EnvironmentSettingsDialog(const EnvironmentSettings &initial,
    QWidget *parent,Preview onPreview,Locate onLocate,TimeProvider clock)
    :SkinnedDialog(QStringLiteral("环境设置"),parent),m(new Impl)
{
    setObjectName(QStringLiteral("environmentSettingsDialog"));
    m->original=initial; m->onPreview=std::move(onPreview);
    m->onLocate=std::move(onLocate); m->clock=std::move(clock);
    auto *body=new QWidget(this); m->shell.setupUi(body);
    body->setStyleSheet(AppTheme::deploymentPanelStyleSheet()+QStringLiteral(
        "QCheckBox::indicator,QRadioButton::indicator{width:14px;height:14px;border:1px solid #92bada;background:#153d66;}"
        "QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#7bd7ef;border:2px solid #e4f6ff;}"
        "QRadioButton::indicator{border-radius:7px;}"));
    contentLayout()->addWidget(body);
    auto add=[&](const QString &label,const QString &icon,auto &ui) {
        auto *page=new QWidget(m->shell.stackedWidget); ui.setupUi(page);
        m->shell.stackedWidget->addWidget(page);
        QIcon pageIcon(QStringLiteral(":/deployment/icons/")+icon);
        if (icon==QStringLiteral("time.png")) {
            QPixmap image(32,32); image.fill(Qt::transparent);
            QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(QColor("#a9dcf8"),2)); painter.setBrush(QColor("#2468bc"));
            painter.drawEllipse(QRectF(3,3,26,26));
            painter.setPen(QPen(Qt::white,2,Qt::SolidLine,Qt::RoundCap));
            painter.drawLine(QPointF(16,8),QPointF(16,16));painter.drawLine(QPointF(16,16),QPointF(22,19));
            painter.end(); pageIcon=QIcon(image);
        }
        auto *item=new QListWidgetItem(pageIcon,label,m->shell.listWidget);
        item->setSizeHint(QSize(106,42));
        return page;
    };
    add(QStringLiteral("光照"),QStringLiteral("sun_light.png"),m->light);
    add(QStringLiteral("雨"),QStringLiteral("rain.png"),m->rain);
    add(QStringLiteral("雪"),QStringLiteral("snow.png"),m->snow);
    auto *fogPage=add(QStringLiteral("雾"),QStringLiteral("fog.png"),m->fog);
    // The old Fog .ui used absolute coordinates; retain its controls but
    // place the group in a layout so it adapts to the new client/DPI scale.
    auto *fogLayout=new QVBoxLayout(fogPage); fogLayout->addWidget(m->fog.groupBox_2); fogLayout->addStretch();
    add(QStringLiteral("时间"),QStringLiteral("time.png"),m->time);
    m->shell.listWidget->setObjectName(QStringLiteral("environmentCategoryList"));
    m->shell.listWidget->setFixedWidth(115); m->shell.listWidget->setIconSize(QSize(24,24));
    m->shell.listWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m->shell.listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->shell.pSplitter->setStretchFactor(0,0);
    m->shell.pSplitter->setChildrenCollapsible(false);
    m->shell.pSplitter->setStretchFactor(1,1);
    m->time.slider_CurrentTime->setRange(0,1439);
    m->time.slider_TimeSpeed->setRange(1,3600);
    m->rain.doubleSpinBox_RainLongitude->setDecimals(6); m->rain.doubleSpinBox_RainLatitude->setDecimals(6);
    m->snow.doubleSpinBox_SnowLongitude->setDecimals(6); m->snow.doubleSpinBox_SnowLatitude->setDecimals(6);
    m->rain.rain_density_doubleSpinBox->setDecimals(2); m->snow.snow_density_doubleSpinBox->setDecimals(2);
    populate(initial);
    const auto pair=[this](QSlider *slider,QDoubleSpinBox *spin,int factor) {
        connect(slider,&QSlider::valueChanged,this,[this,spin,factor](int v){ QSignalBlocker block(spin);spin->setValue(double(v)/factor);preview(); });
        connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,slider,factor](double v){ QSignalBlocker block(slider);slider->setValue(qRound(v*factor));preview(); });
    };
    pair(m->rain.hSlider_RainIntensity,m->rain.rain_density_doubleSpinBox,10);
    pair(m->snow.hSlider_SnowIntensity,m->snow.snow_density_doubleSpinBox,10);
    pair(m->fog.hSlider_FogIntensity,m->fog.Fog_density_doubleSpinBox,100);
    connect(m->light.sun_light_Slider,&QSlider::valueChanged,this,[this](int v){QSignalBlocker b(m->light.spinBox_Light);m->light.spinBox_Light->setValue(v);preview();});
    connect(m->light.spinBox_Light,qOverload<int>(&QSpinBox::valueChanged),this,[this](int v){QSignalBlocker b(m->light.sun_light_Slider);m->light.sun_light_Slider->setValue(v);preview();});
    for(auto *box:body->findChildren<QCheckBox*>())
        if(box!=m->time.checkBox_SyncTime)connect(box,&QCheckBox::toggled,this,[this]{preview();});
    for(auto *box:body->findChildren<QRadioButton*>())connect(box,&QRadioButton::toggled,this,[this]{preview();});
    for(auto *box:{m->rain.doubleSpinBox_RainHeight,m->rain.doubleSpinBox_RainLatitude,
        m->rain.doubleSpinBox_RainLongitude,m->rain.doubleSpinBox_RainRadius,
        m->snow.doubleSpinBox_SnowHeight,m->snow.doubleSpinBox_SnowLatitude,
        m->snow.doubleSpinBox_SnowLongitude,m->snow.doubleSpinBox_SnowRadius})
        connect(box,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{preview();});
    connect(m->rain.toolBtn_Locate,&QToolButton::clicked,this,[this]{if(m->onLocate)m->onLocate(m->rain.doubleSpinBox_RainLongitude->value(),m->rain.doubleSpinBox_RainLatitude->value(),m->rain.doubleSpinBox_RainHeight->value());});
    connect(m->snow.toolBtn_Locate,&QToolButton::clicked,this,[this]{if(m->onLocate)m->onLocate(m->snow.doubleSpinBox_SnowLongitude->value(),m->snow.doubleSpinBox_SnowLatitude->value(),m->snow.doubleSpinBox_SnowHeight->value());});
    connect(m->time.slider_CurrentTime,&QSlider::valueChanged,this,[this](int minutes){
        m->requestedTime.setTime(QTime(minutes/60,minutes%60)); preview(true);
    });
    connect(m->time.slider_TimeSpeed,&QSlider::valueChanged,this,[this]{preview();});
    connect(m->time.toolBtn_StartPause,&QToolButton::clicked,this,[this]{
        if(m->clock && m->clock().isValid())m->requestedTime=m->clock();
        m->running=!m->running; preview(true);
    });
    connect(m->time.toolBtn_Reset,&QToolButton::clicked,this,[this]{m->requestedTime=QDateTime::currentDateTime();preview(true);});
    connect(m->time.checkBox_SyncTime,&QCheckBox::toggled,this,[this](bool sync){
        if(m->populating)return;
        if(m->clock && m->clock().isValid())m->requestedTime=m->clock();
        if(sync){m->running=false;m->requestedTime=QDateTime::currentDateTime();} preview(true);
    });
    connect(m->shell.listWidget,&QListWidget::currentRowChanged,m->shell.stackedWidget,&QStackedWidget::setCurrentIndex);
    connect(m->shell.pBtnOK,&QToolButton::clicked,this,&QDialog::accept);
    connect(m->shell.pBtnCancel,&QToolButton::clicked,this,&QDialog::reject);
    connect(m->shell.pBtnReset,&QToolButton::clicked,this,[this]{populate(EnvironmentSettings{});preview(true);});
    auto *timer=new QTimer(this); timer->setInterval(250);
    connect(timer,&QTimer::timeout,this,[this]{updateTimeLabel();});timer->start();
    selectPage(0); updateEnabled(); resize(620,540);
    m->shell.pSplitter->setSizes({115,480});
}
EnvironmentSettingsDialog::~EnvironmentSettingsDialog()=default;
void EnvironmentSettingsDialog::selectPage(int index){m->shell.listWidget->setCurrentRow(qBound(0,index,4));}
void EnvironmentSettingsDialog::populate(const EnvironmentSettings &v)
{
    m->populating=true;m->requestedTime=timeOf(v);m->running=v.animateTime;
    m->light.sun_checkBox->setChecked(v.sunVisible);m->light.Atmosphere_checkBox->setChecked(v.atmosphereVisible);
    m->light.moon_checkBox->setChecked(v.moonVisible);m->light.star_checkBox->setChecked(v.starsVisible);m->light.nebula_checkBox->setChecked(v.nebulaVisible);
    m->light.spinBox_Light->setValue(qRound(v.ambientIntensity*2.55));m->light.sun_light_Slider->setValue(qRound(v.ambientIntensity*2.55));
    m->rain.checkBox_Rain->setChecked(v.rainVisible);m->rain.rain_density_doubleSpinBox->setValue(v.rain/100.0);
    m->rain.hSlider_RainIntensity->setValue(qRound(v.rain/10.0));m->rain.doubleSpinBox_RainHeight->setValue(v.rainHeightMeters);
    m->rain.raduiBtn_GlobalRain->setChecked(v.rainGlobal);m->rain.raduiBtn_LocalRain->setChecked(!v.rainGlobal);
    m->rain.doubleSpinBox_RainLongitude->setValue(v.rainLongitude);m->rain.doubleSpinBox_RainLatitude->setValue(v.rainLatitude);m->rain.doubleSpinBox_RainRadius->setValue(v.rainRadiusMeters);
    m->snow.checkBox_Snow->setChecked(v.snowVisible);m->snow.snow_density_doubleSpinBox->setValue(v.snow/100.0);
    m->snow.hSlider_SnowIntensity->setValue(qRound(v.snow/10.0));m->snow.doubleSpinBox_SnowHeight->setValue(v.snowHeightMeters);
    m->snow.radioBtn_GlobalSnow->setChecked(v.snowGlobal);m->snow.radioBtn_LocalSnow->setChecked(!v.snowGlobal);
    m->snow.doubleSpinBox_SnowLongitude->setValue(v.snowLongitude);m->snow.doubleSpinBox_SnowLatitude->setValue(v.snowLatitude);m->snow.doubleSpinBox_SnowRadius->setValue(v.snowRadiusMeters);
    m->fog.checkBox_Fog->setChecked(v.fogVisible);m->fog.hSlider_FogIntensity->setValue(v.fog);m->fog.Fog_density_doubleSpinBox->setValue(v.fog/100.0);
    m->time.slider_TimeSpeed->setValue(v.timeSpeed);m->time.checkBox_SyncTime->setChecked(v.syncSystemTime);
    {QSignalBlocker b(m->time.slider_CurrentTime);m->time.slider_CurrentTime->setValue(v.timeOfDay*60+v.minuteOfHour);}
    m->populating=false;updateEnabled();updateTimeLabel();
}
EnvironmentSettings EnvironmentSettingsDialog::settings() const
{
    auto v=m->original;putTime(v,m->requestedTime);
    v.sunVisible=m->light.sun_checkBox->isChecked();v.atmosphereVisible=m->light.Atmosphere_checkBox->isChecked();
    v.moonVisible=m->light.moon_checkBox->isChecked();v.starsVisible=m->light.star_checkBox->isChecked();v.nebulaVisible=m->light.nebula_checkBox->isChecked();
    v.ambientIntensity=m->light.spinBox_Light->value()*100.0/255.0;
    v.rainVisible=m->rain.checkBox_Rain->isChecked();v.rain=qRound(m->rain.rain_density_doubleSpinBox->value()*100);
    v.rainGlobal=m->rain.raduiBtn_GlobalRain->isChecked();v.rainLongitude=m->rain.doubleSpinBox_RainLongitude->value();v.rainLatitude=m->rain.doubleSpinBox_RainLatitude->value();
    v.rainRadiusMeters=m->rain.doubleSpinBox_RainRadius->value();v.rainHeightMeters=m->rain.doubleSpinBox_RainHeight->value();
    v.snowVisible=m->snow.checkBox_Snow->isChecked();v.snow=qRound(m->snow.snow_density_doubleSpinBox->value()*100);
    v.snowGlobal=m->snow.radioBtn_GlobalSnow->isChecked();v.snowLongitude=m->snow.doubleSpinBox_SnowLongitude->value();v.snowLatitude=m->snow.doubleSpinBox_SnowLatitude->value();
    v.snowRadiusMeters=m->snow.doubleSpinBox_SnowRadius->value();v.snowHeightMeters=m->snow.doubleSpinBox_SnowHeight->value();
    v.fogVisible=m->fog.checkBox_Fog->isChecked();v.fog=qRound(m->fog.Fog_density_doubleSpinBox->value()*100);
    v.syncSystemTime=m->time.checkBox_SyncTime->isChecked();v.animateTime=m->running&&!v.syncSystemTime;v.timeSpeed=m->time.slider_TimeSpeed->value();
    return v;
}
EnvironmentSettings EnvironmentSettingsDialog::snapshot() const
{
    auto v=settings();if(m->clock && m->clock().isValid())putTime(v,m->clock());return v;
}
void EnvironmentSettingsDialog::preview(bool resetClock)
{
    if(m->populating)return;
    if(resetClock)++m->original.clockRevision;
    updateEnabled();if(m->onPreview)m->onPreview(settings());updateTimeLabel();
}
void EnvironmentSettingsDialog::updateTimeLabel()
{
    QDateTime t=m->requestedTime;
    if(m->clock && m->clock().isValid())t=m->clock();
    m->time.label_CurrentTime->setText(t.toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss")));
    QSignalBlocker block(m->time.slider_CurrentTime);m->time.slider_CurrentTime->setValue(t.time().hour()*60+t.time().minute());
    m->time.label_speedData->setText(QStringLiteral("%1 倍").arg(m->time.slider_TimeSpeed->value()));
}
void EnvironmentSettingsDialog::updateEnabled()
{
    const auto v=settings();
    m->light.spinBox_Light->setEnabled(v.sunVisible);m->light.sun_light_Slider->setEnabled(v.sunVisible);
    m->rain.rain_density_doubleSpinBox->setEnabled(v.rainVisible);m->rain.hSlider_RainIntensity->setEnabled(v.rainVisible);m->rain.doubleSpinBox_RainHeight->setEnabled(v.rainVisible);
    m->rain.groupBox_3->setEnabled(!v.rainGlobal);m->rain.toolBtn_Locate->setEnabled(!v.rainGlobal);
    m->snow.snow_density_doubleSpinBox->setEnabled(v.snowVisible);m->snow.hSlider_SnowIntensity->setEnabled(v.snowVisible);m->snow.doubleSpinBox_SnowHeight->setEnabled(v.snowVisible);
    m->snow.groupBox_3->setEnabled(!v.snowGlobal);m->snow.toolBtn_Locate->setEnabled(!v.snowGlobal);
    m->fog.Fog_density_doubleSpinBox->setEnabled(v.fogVisible);m->fog.hSlider_FogIntensity->setEnabled(v.fogVisible);
    m->time.toolBtn_StartPause->setText(v.animateTime?QStringLiteral("暂停"):QStringLiteral("开始"));
    m->time.toolBtn_StartPause->setEnabled(!v.syncSystemTime);m->time.slider_TimeSpeed->setEnabled(!v.syncSystemTime);
    m->time.slider_CurrentTime->setEnabled(!v.syncSystemTime&&!v.animateTime);
    m->time.toolBtn_Reset->setEnabled(!v.syncSystemTime&&!v.animateTime);m->time.checkBox_SyncTime->setEnabled(!v.animateTime);
}
}
