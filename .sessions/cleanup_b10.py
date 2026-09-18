import re

# 头文件
h_path = r'E:\WakeAI\include\system\DatabaseManager.h'
with open(h_path, 'r', encoding='utf-8') as f:
    h = f.read()
h = h.replace('    QVector<QString> unlockedAchievements() const;\n', '')
with open(h_path, 'w', encoding='utf-8', newline='') as f:
    f.write(h)
print('header done')

# cpp 文件
cpp_path = r'E:\WakeAI\src\system\DatabaseManager.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    cpp = f.read()

old = '''QVector<QString> DatabaseManager::unlockedAchievements() const {
    QVector<QString> ids; if(!isOpen()) return ids;
    QSqlQuery q(db_);
    if(q.exec("SELECT achievement_id FROM achievements WHERE unlocked=1 ORDER BY id"))
        while(q.next()) ids.append(q.value(0).toString());
    else error_=q.lastError().text();
    return ids;
}
'''
cpp = cpp.replace(old, '')
with open(cpp_path, 'w', encoding='utf-8', newline='') as f:
    f.write(cpp)
print('cpp done')
