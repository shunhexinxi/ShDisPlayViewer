#include "modules/zqbs/EnvironmentSettingsDialog.h"
#include "modules/zqbs/DeploymentXmlStore.h"
#include "app/AppPaths.h"
#include "ui/AppTheme.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <cmath>
#include <QTemporaryDir>
#include <QToolButton>
#include <iostream>
#define CHECK(c) do { if (!(c)) {std::cerr<<"FAIL line "<<__LINE__<<": "<<#c<<'\n'; return 1;} } while(false)
template<class T> T *control(QWidget &w,const char *n){return w.findChild<T*>(QString::fromLatin1(n));}
int main(int argc,char **argv) {
    QApplication app(argc,argv); Zqxxxt::AppTheme::apply(app);
    Zqxxxt::AppPaths::initialize(QCoreApplication::applicationDirPath());
    Zqxxxt::EnvironmentSettings initial;
    initial.calendarDate=QStringLiteral("2026-09-09");initial.timeOfDay=12;initial.minuteOfHour=34;initial.secondOfMinute=56;
    QDateTime time(QDate(2026,9,9),QTime(12,34,56));int previews=0,located=0;
    Zqxxxt::EnvironmentSettings live;
    Zqxxxt::EnvironmentSettingsDialog dialog(initial,nullptr,
        [&](const auto &v){live=v;++previews;time=QDateTime(QDate::fromString(v.calendarDate,Qt::ISODate),QTime(v.timeOfDay,v.minuteOfHour,v.secondOfMinute));},
        [&](double lon,double lat,double height){if(lon==120.25&&lat==36.125&&height==20000)++located;},
        [&]{return time;});
    dialog.setWindowModality(Qt::NonModal);dialog.show();app.processEvents();
    CHECK(!dialog.isModal());
    auto *list=control<QListWidget>(dialog,"environmentCategoryList");CHECK(list&&list->count()==5);
    QStringList names{QStringLiteral("光照"),QStringLiteral("雨"),QStringLiteral("雪"),QStringLiteral("雾"),QStringLiteral("时间")};
    for(int i=0;i<5;++i) {
        CHECK(list->item(i)->text()==names[i]&&!list->item(i)->icon().isNull());
        dialog.selectPage(i);app.processEvents();
        if(argc>1)CHECK(dialog.grab().save(QDir(QString::fromLocal8Bit(argv[1])).filePath(QString("environment-%1.png").arg(i))));
    }
    control<QCheckBox>(dialog,"sun_checkBox")->setChecked(true);
    for(int level=0;level<=255;++level) {
        control<QSpinBox>(dialog,"spinBox_Light")->setValue(level);
        CHECK(control<QSlider>(dialog,"sun_light_Slider")->value()==level);
        CHECK(std::abs(live.ambientIntensity-level*100.0/255.0)<1e-9);
    }
    control<QSpinBox>(dialog,"spinBox_Light")->setValue(127);
    control<QCheckBox>(dialog,"checkBox_Rain")->setChecked(true);
    auto *rain=control<QDoubleSpinBox>(dialog,"rain_density_doubleSpinBox");
    rain->setValue(1);CHECK(live.rain==100&&live.rainVisible&&live.clockRevision==initial.clockRevision);
    control<QSlider>(dialog,"hSlider_RainIntensity")->setValue(7);CHECK(rain->value()==.7&&live.rain==70);
    control<QRadioButton>(dialog,"raduiBtn_LocalRain")->setChecked(true);CHECK(!live.rainGlobal);
    control<QDoubleSpinBox>(dialog,"doubleSpinBox_RainLongitude")->setValue(120.25);
    control<QDoubleSpinBox>(dialog,"doubleSpinBox_RainLatitude")->setValue(36.125);
    control<QDoubleSpinBox>(dialog,"doubleSpinBox_RainHeight")->setValue(20000);
    control<QToolButton>(dialog,"toolBtn_Locate")->click();CHECK(located==1);
    control<QCheckBox>(dialog,"checkBox_Snow")->setChecked(true);
    control<QDoubleSpinBox>(dialog,"snow_density_doubleSpinBox")->setValue(1);CHECK(live.snow==100&&live.snowVisible);
    control<QCheckBox>(dialog,"checkBox_Fog")->setChecked(true);
    control<QDoubleSpinBox>(dialog,"Fog_density_doubleSpinBox")->setValue(.6);CHECK(live.fog==60&&live.fogVisible);
    control<QSlider>(dialog,"slider_CurrentTime")->setValue(1439);CHECK(live.timeOfDay==23&&live.minuteOfHour==59&&live.clockRevision>initial.clockRevision);
    control<QSlider>(dialog,"slider_TimeSpeed")->setValue(3600);
    control<QToolButton>(dialog,"toolBtn_StartPause")->click();CHECK(live.animateTime&&live.timeSpeed==3600);
    CHECK(!control<QSlider>(dialog,"slider_CurrentTime")->isEnabled());
    control<QToolButton>(dialog,"toolBtn_StartPause")->click();CHECK(!live.animateTime);
    control<QCheckBox>(dialog,"checkBox_SyncTime")->setChecked(true);CHECK(live.syncSystemTime&&!live.animateTime);
    CHECK(!control<QToolButton>(dialog,"toolBtn_StartPause")->isEnabled());
    QTemporaryDir tmp;CHECK(tmp.isValid());
    Zqxxxt::DeploymentXmlStore store(tmp.path());Zqxxxt::DeploymentData data;QString error;
    CHECK(store.load(data,error)); data.environment=dialog.snapshot();
    CHECK(store.saveFixed(data,error));
    Zqxxxt::DeploymentData restored;CHECK(store.load(restored,error));
    CHECK(restored.environment.calendarDate==data.environment.calendarDate);
    CHECK(std::abs(restored.environment.ambientIntensity-127*100.0/255.0)<1e-9);
    CHECK(restored.environment.secondOfMinute==data.environment.secondOfMinute);
    CHECK(restored.environment.syncSystemTime&&restored.environment.timeSpeed==3600);
    CHECK(restored.environment.rain==70&&!restored.environment.rainGlobal&&restored.environment.fog==60);
    CHECK(previews>10);std::cout<<"PASS reference five pages, icons, controls, preview, time and XML persistence\n";return 0;
}
