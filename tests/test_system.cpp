#include "system/DatabaseManager.h"
#include "system/AchievementEngine.h"
#include "system/AlarmManager.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
using namespace wakeai;
void waitMs(int n){QEventLoop loop;QTimer::singleShot(n,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char* argv[]) {
    QCoreApplication app(argc,argv);QTemporaryDir dir;
    int fails=0;
    auto check=[&](bool ok,const char* text){std::cout<<(ok?"PASS ":"FAIL ")<<text<<'\n';if(!ok)++fails;};
    check(dir.isValid(),"temporary test directory");
    const auto path=dir.filePath("test.db");
    { // Seed V1 records, then verify additive migration preserves them.
        auto old=QSqlDatabase::addDatabase("QSQLITE","migration_seed");old.setDatabaseName(path);
        check(old.open(),"legacy db open");
        {QSqlQuery q(old);
         check(q.exec("CREATE TABLE alarm_settings(id INTEGER PRIMARY KEY AUTOINCREMENT,hour INTEGER NOT NULL,"
             "minute INTEGER NOT NULL,exercise_type TEXT NOT NULL,target_count INTEGER NOT NULL,"
             "enabled INTEGER NOT NULL DEFAULT 0,theme TEXT NOT NULL DEFAULT 'default')"),"legacy alarm table");
         check(q.exec("INSERT INTO alarm_settings VALUES(1,7,30,'squat',12,0,'default')"),"legacy alarm row");
         check(q.exec("CREATE TABLE wake_records(id INTEGER PRIMARY KEY AUTOINCREMENT,date TEXT NOT NULL,alarm_time TEXT NOT NULL,"
             "exercise_type TEXT NOT NULL,target_count INTEGER NOT NULL,actual_count INTEGER NOT NULL,success INTEGER NOT NULL,completed_at TEXT NOT NULL)"),"legacy table");
         check(q.exec("INSERT INTO wake_records VALUES(1,'2000-01-01','07:00','squat',1,1,1,'2000-01-01 07:01:00')"),"legacy record");}
        old.close();
    }
    QSqlDatabase::removeDatabase("migration_seed");
    {
        DatabaseManager db(path);check(db.init(),"init and migrate");
        check(db.recentRecords().size()==1,"legacy record preserved");
        auto migrated=db.loadSettings();
        check(migrated.hour==7&&migrated.ringtoneId=="builtin:classic"&&migrated.volume==0.85,
              "legacy alarm gains ringtone defaults");
        auto multi=db.alarms();
        check(multi.size()==1&&multi.first().hour==7&&multi.first().minute==30,
              "legacy single alarm imported into multi-alarm table");
        AlarmSetting workday;workday.hour=6;workday.minute=45;workday.label="Workday";
        workday.enabled=true;workday.repeatMask=0x1F;workday.snoozeMinutes=8;
        workday.exerciseType="jumping_jack";workday.targetCount=25;
        auto workdayId=db.saveAlarm(workday);
        AlarmSetting weekend=workday;weekend.hour=9;weekend.minute=10;weekend.label="Weekend";
        weekend.repeatMask=0x60;weekend.enabled=false;
        auto weekendId=db.saveAlarm(weekend);
        check(workdayId>0&&weekendId>workdayId&&db.alarms().size()==3,
              "multiple alarms have independent identifiers");
        auto workdayLoaded=db.alarm(workdayId);
        check(workdayLoaded.label=="Workday"&&workdayLoaded.repeatMask==0x1F
              &&workdayLoaded.snoozeMinutes==8&&workdayLoaded.targetCount==25,
              "multi-alarm fields round trip");
        check(db.setAlarmEnabled(weekendId,true)&&db.alarm(weekendId).enabled,
              "alarm enable switch persists");
        workdayLoaded.minute=50;workdayLoaded.label="Updated";
        check(db.saveAlarm(workdayLoaded)==workdayId&&db.alarm(workdayId).minute==50,
              "existing alarm updates in place");
        check(db.deleteAlarm(weekendId)&&db.alarm(weekendId).id<0,
              "alarm delete removes only selected alarm");
        check(db.consecutiveWakeDaysAt(QDate(2026,9,7))==0,"old streak is not current streak");
        AlarmSetting a;a.hour=8;a.minute=15;a.targetCount=20;a.enabled=true;
        a.ringtoneId="custom:test.wav";a.volume=0.64;
        check(db.saveAlarmSetting(a),"save setting");auto loaded=db.loadSettings();
        check(loaded.hour==8&&loaded.minute==15&&loaded.targetCount==20&&loaded.enabled
              &&loaded.ringtoneId=="custom:test.wav"&&loaded.volume==0.64,"setting round trip");
        a.hour=99;check(!db.saveAlarmSetting(a)&&db.loadSettings().hour==8,"invalid setting preserves old row");
        auto add=[&](QString id,QString date,int count){WakeRecord r;
            r.date=date;r.alarmTime="08:15";r.exerciseType="squat";r.targetCount=count;r.actualCount=count;r.success=true;
            r.completedAt=date+" 08:16:00";r.sessionId=id;return db.saveWakeRecord(r);};
        check(add("one","2026-09-05",20)&&add("one","2026-09-05",20),"idempotent record retry");
        check(db.queryStatistics().totalWakeCount==2,"retry does not duplicate record");
        check(add("two","2026-09-06",40)&&add("three","2026-09-07",40),"add consecutive dates");
        check(db.consecutiveWakeDaysAt(QDate(2026,9,7))==3,"three-day streak");
        check(db.consecutiveWakeDaysAt(QDate(2026,9,8))==3,"yesterday streak still active");
        check(db.consecutiveWakeDaysAt(QDate(2026,9,9))==0,"missed day resets streak");
        auto stats=db.queryStatistics();stats.consecutiveDays=3;
        check(stats.totalWakeCount==4&&stats.totalActions==101,"count and action totals");
        AchievementEngine engine(db);auto ids=engine.checkAndUnlock(stats);
        check(ids.size()==3,"first wake, three day, 100 actions unlock");
        check(engine.checkAndUnlock(stats).isEmpty(),"no duplicate achievement");
        DatabaseManager independent(dir.filePath("other.db"));check(independent.init(),"independent connection");
        independent.close();check(db.isOpen()&&db.recentRecords().size()==4,"closing another db does not break first");
    }
    {DatabaseManager reopened(path);check(reopened.init()&&reopened.recentRecords().size()==4,"records persist on reopen");}
    AlarmManager alarm;int triggered=0;
    QObject::connect(&alarm,&AlarmManager::alarmTriggered,[&]{++triggered;});
    alarm.setTestAlarmInSeconds(1);alarm.enable();waitMs(1500);
    check(alarm.isRinging()&&triggered==1,"test alarm triggers once");
    check(!alarm.stop(),"unfinished challenge cannot stop");
    alarm.disable();check(alarm.isRinging(),"disable cannot bypass active challenge");
    check(alarm.snooze(1)&&!alarm.isRinging(),"explicit snooze");waitMs(1500);
    check(alarm.isRinging()&&triggered==2,"snooze triggers again");
    alarm.setChallengeCompleted(true);check(alarm.stop()&&!alarm.isEnabled(),"finish stops one-shot alarm");
    waitMs(500);check(triggered==2,"no repeated trigger after completion");
    alarm.setTestAlarmInSeconds(1);alarm.enable();alarm.disable();waitMs(1500);
    check(triggered==2&&!alarm.isRinging(),"cancel also cancels test timer");
    alarm.setAlarmDateTime(QDateTime::currentDateTime().addMSecs(450));alarm.enable();waitMs(800);
    check(alarm.isRinging()&&triggered==3,"exact date-time scheduling triggers selected occurrence");
    alarm.setChallengeCompleted(true);check(alarm.stop(),"exact occurrence can complete normally");
    std::cout<<"Failures: "<<fails<<'\n';return fails?1:0;
}
