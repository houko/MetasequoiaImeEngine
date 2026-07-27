#include "user_dictionary_journal.h"

#include <sqlite3.h>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

namespace user_dictionary
{
namespace
{
struct DbCloser
{
    void operator()(sqlite3 *db) const
    {
        if (db != nullptr) sqlite3_close(db);
    }
};
using Db = std::unique_ptr<sqlite3, DbCloser>;

struct StmtCloser
{
    void operator()(sqlite3_stmt *stmt) const
    {
        if (stmt != nullptr) sqlite3_finalize(stmt);
    }
};
using Stmt = std::unique_ptr<sqlite3_stmt, StmtCloser>;

const char *kind_name(DictionaryKind kind)
{
    switch (kind)
    {
    case DictionaryKind::Pinyin: return "pinyin";
    case DictionaryKind::Wubi: return "wubi";
    case DictionaryKind::QuickPhrase: return "quick";
    case DictionaryKind::English: return "english";
    }
    return "";
}

Db open_database(const std::string &path, int flags)
{
    sqlite3 *raw = nullptr;
    if (sqlite3_open_v2(path.c_str(), &raw, flags | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        if (raw != nullptr) sqlite3_close(raw);
        return {};
    }
    sqlite3_busy_timeout(raw, 5000);
    return Db(raw);
}

Stmt prepare(sqlite3 *db, const std::string &sql)
{
    sqlite3_stmt *raw = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) return {};
    return Stmt(raw);
}

bool bind_text(sqlite3_stmt *stmt, int index, const std::string &value)
{
    return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool ensure_schema(sqlite3 *db)
{
    constexpr const char *sql =
        "CREATE TABLE IF NOT EXISTS user_dictionary_operations("
        "dictionary TEXT NOT NULL,"
        "key TEXT NOT NULL,"
        "value TEXT NOT NULL,"
        "operation TEXT NOT NULL CHECK(operation IN ('upsert','delete')),"
        "weight INTEGER NOT NULL DEFAULT 0,"
        "display TEXT NOT NULL DEFAULT '',"
        "updated_at INTEGER NOT NULL DEFAULT(unixepoch()),"
        "PRIMARY KEY(dictionary,key,value));"
        "PRAGMA user_version=1;";
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::vector<std::string> pinyin_segments(const std::string &key)
{
    std::vector<std::string> segments;
    size_t start = 0;
    while (start <= key.size())
    {
        const size_t delimiter = key.find('\'', start);
        const std::string segment = key.substr(start, delimiter == std::string::npos ? std::string::npos
                                                                                    : delimiter - start);
        if (segment.empty()) return {};
        segments.push_back(segment);
        if (delimiter == std::string::npos) break;
        start = delimiter + 1;
    }
    return segments;
}

std::string pinyin_table(const std::string &key)
{
    const auto segments = pinyin_segments(key);
    return segments.empty() ? std::string{} :
           "tbl_" + std::to_string(segments.size()) + "_" + std::string(1, segments.front().front());
}

std::string jianpin(const std::vector<std::string> &segments)
{
    std::string result;
    result.reserve(segments.size());
    for (const auto &segment : segments) result.push_back(segment.front());
    return result;
}

bool apply_pinyin(sqlite3 *db, const std::string &key, const std::string &value, const std::string &operation,
                  int weight)
{
    const auto segments = pinyin_segments(key);
    if (segments.empty()) return false;
    const std::string table = pinyin_table(key);
    if (operation == "delete")
    {
        auto stmt = prepare(db, "DELETE FROM \"" + table + "\" WHERE key=?1 AND value=?2");
        return stmt && bind_text(stmt.get(), 1, key) && bind_text(stmt.get(), 2, value) &&
               sqlite3_step(stmt.get()) == SQLITE_DONE;
    }

    auto update = prepare(db, "UPDATE \"" + table + "\" SET jp=?1,weight=?2 WHERE key=?3 AND value=?4");
    if (!update || !bind_text(update.get(), 1, jianpin(segments)) ||
        sqlite3_bind_int(update.get(), 2, weight) != SQLITE_OK || !bind_text(update.get(), 3, key) ||
        !bind_text(update.get(), 4, value) || sqlite3_step(update.get()) != SQLITE_DONE)
        return false;
    if (sqlite3_changes(db) > 0) return true;

    auto insert = prepare(db, "INSERT INTO \"" + table + "\"(key,jp,value,weight) VALUES(?1,?2,?3,?4)");
    return insert && bind_text(insert.get(), 1, key) &&
           bind_text(insert.get(), 2, jianpin(segments)) &&
           bind_text(insert.get(), 3, value) && sqlite3_bind_int(insert.get(), 4, weight) == SQLITE_OK &&
           sqlite3_step(insert.get()) == SQLITE_DONE;
}

bool apply_simple(sqlite3 *db, const std::string &table, const std::string &key_column,
                  const std::string &value_column, const std::string &key, const std::string &value,
                  const std::string &operation, int weight)
{
    if (operation == "delete")
    {
        auto stmt = prepare(db, "DELETE FROM \"" + table + "\" WHERE \"" + key_column +
                                   "\"=?1 AND \"" + value_column + "\"=?2");
        return stmt && bind_text(stmt.get(), 1, key) && bind_text(stmt.get(), 2, value) &&
               sqlite3_step(stmt.get()) == SQLITE_DONE;
    }
    auto update = prepare(db, "UPDATE \"" + table + "\" SET weight=?1 WHERE \"" + key_column +
                                  "\"=?2 AND \"" + value_column + "\"=?3");
    if (!update || sqlite3_bind_int(update.get(), 1, weight) != SQLITE_OK || !bind_text(update.get(), 2, key) ||
        !bind_text(update.get(), 3, value) || sqlite3_step(update.get()) != SQLITE_DONE)
        return false;
    if (sqlite3_changes(db) > 0) return true;
    auto insert = prepare(db, "INSERT INTO \"" + table + "\"(\"" + key_column + "\",\"" + value_column +
                                  "\",weight) VALUES(?1,?2,?3)");
    return insert && bind_text(insert.get(), 1, key) && bind_text(insert.get(), 2, value) &&
           sqlite3_bind_int(insert.get(), 3, weight) == SQLITE_OK && sqlite3_step(insert.get()) == SQLITE_DONE;
}

bool apply_english(sqlite3 *db, const std::string &key, const std::string &operation, const std::string &display)
{
    if (operation == "delete")
    {
        auto stmt = prepare(db, "DELETE FROM english_words WHERE word=?1");
        return stmt && bind_text(stmt.get(), 1, key) && sqlite3_step(stmt.get()) == SQLITE_DONE;
    }
    auto update = prepare(db, "UPDATE english_words SET display=?1 WHERE word=?2");
    if (!update || !bind_text(update.get(), 1, display) || !bind_text(update.get(), 2, key) ||
        sqlite3_step(update.get()) != SQLITE_DONE)
        return false;
    if (sqlite3_changes(db) > 0) return true;
    auto insert = prepare(db, "INSERT INTO english_words(word,display) VALUES(?1,?2)");
    return insert && bind_text(insert.get(), 1, key) && bind_text(insert.get(), 2, display) &&
           sqlite3_step(insert.get()) == SQLITE_DONE;
}
} // namespace

std::string default_user_db_path()
{
    char *local_app_data = nullptr;
    size_t length = 0;
    const errno_t error = _dupenv_s(&local_app_data, &length, "LOCALAPPDATA");
    const std::string base = error == 0 && local_app_data != nullptr ? local_app_data : "";
    free(local_app_data);
    return base + "\\metasequoiaime\\msime_user.db";
}

bool record_upsert(const std::string &user_db_path, DictionaryKind kind, const std::string &key,
                   const std::string &value, int weight, const std::string &display)
{
    auto db = open_database(user_db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    if (!db || !ensure_schema(db.get())) return false;
    auto stmt = prepare(db.get(),
                        "INSERT INTO user_dictionary_operations(dictionary,key,value,operation,weight,display)"
                        " VALUES(?1,?2,?3,'upsert',?4,?5)"
                        " ON CONFLICT(dictionary,key,value) DO UPDATE SET operation='upsert',weight=excluded.weight,"
                        " display=excluded.display,updated_at=unixepoch()");
    return stmt && bind_text(stmt.get(), 1, kind_name(kind)) && bind_text(stmt.get(), 2, key) &&
           bind_text(stmt.get(), 3, value) && sqlite3_bind_int(stmt.get(), 4, weight) == SQLITE_OK &&
           bind_text(stmt.get(), 5, display) && sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool record_delete(const std::string &user_db_path, DictionaryKind kind, const std::string &key,
                   const std::string &value)
{
    auto db = open_database(user_db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    if (!db || !ensure_schema(db.get())) return false;
    auto stmt = prepare(db.get(),
                        "INSERT INTO user_dictionary_operations(dictionary,key,value,operation)"
                        " VALUES(?1,?2,?3,'delete')"
                        " ON CONFLICT(dictionary,key,value) DO UPDATE SET operation='delete',weight=0,display='',"
                        " updated_at=unixepoch()");
    return stmt && bind_text(stmt.get(), 1, kind_name(kind)) && bind_text(stmt.get(), 2, key) &&
           bind_text(stmt.get(), 3, value) && sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool record_pinyin_upsert_from_database(const std::string &main_db_path, const std::string &key,
                                        const std::string &value)
{
    auto db = open_database(main_db_path, SQLITE_OPEN_READONLY);
    const std::string table = pinyin_table(key);
    if (!db || table.empty()) return false;
    auto stmt = prepare(db.get(), "SELECT weight FROM \"" + table + "\" WHERE key=?1 AND value=?2 LIMIT 1");
    if (!stmt || !bind_text(stmt.get(), 1, key) || !bind_text(stmt.get(), 2, value) ||
        sqlite3_step(stmt.get()) != SQLITE_ROW)
        return false;
    return record_upsert(default_user_db_path(), DictionaryKind::Pinyin, key, value,
                         sqlite3_column_int(stmt.get(), 0));
}

ReplayResult replay(const std::string &user_db_path, const std::string &main_db_path,
                    const std::string &english_db_path)
{
    ReplayResult result;
    if (!std::filesystem::exists(user_db_path)) return result;
    auto journal = open_database(user_db_path, SQLITE_OPEN_READONLY);
    if (!journal)
    {
        result.error = "cannot open user dictionary database";
        return result;
    }
    auto main_db = open_database(main_db_path, SQLITE_OPEN_READWRITE);
    auto english_db = open_database(english_db_path, SQLITE_OPEN_READWRITE);
    if (!main_db || !english_db)
    {
        result.error = "cannot open target dictionary database";
        return result;
    }
    auto rows = prepare(journal.get(),
                        "SELECT dictionary,key,value,operation,weight,display"
                        " FROM user_dictionary_operations ORDER BY updated_at,rowid");
    if (!rows)
    {
        result.error = "invalid user dictionary database";
        return result;
    }

    sqlite3_exec(main_db.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    sqlite3_exec(english_db.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    while (sqlite3_step(rows.get()) == SQLITE_ROW)
    {
        const std::string kind = reinterpret_cast<const char *>(sqlite3_column_text(rows.get(), 0));
        const std::string key = reinterpret_cast<const char *>(sqlite3_column_text(rows.get(), 1));
        const std::string value = reinterpret_cast<const char *>(sqlite3_column_text(rows.get(), 2));
        const std::string operation = reinterpret_cast<const char *>(sqlite3_column_text(rows.get(), 3));
        const int weight = sqlite3_column_int(rows.get(), 4);
        const std::string display = reinterpret_cast<const char *>(sqlite3_column_text(rows.get(), 5));
        bool ok = false;
        if (kind == "pinyin") ok = apply_pinyin(main_db.get(), key, value, operation, weight);
        else if (kind == "wubi") ok = apply_simple(main_db.get(), "wubi86", "key", "value", key, value, operation, weight);
        else if (kind == "quick") ok = apply_simple(main_db.get(), "quick_parases", "key", "value", key, value, operation, weight);
        else if (kind == "english") ok = apply_english(english_db.get(), key, operation, display);
        ok ? ++result.applied : ++result.failed;
    }
    sqlite3_exec(main_db.get(), result.failed == 0 ? "COMMIT" : "ROLLBACK", nullptr, nullptr, nullptr);
    sqlite3_exec(english_db.get(), result.failed == 0 ? "COMMIT" : "ROLLBACK", nullptr, nullptr, nullptr);
    if (result.failed != 0) result.error = "one or more operations failed; changes were rolled back";
    return result;
}
} // namespace user_dictionary
