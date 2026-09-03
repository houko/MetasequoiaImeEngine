#include "../../core/ime_session.h"
#include "../../core/data_path.h"
#include "../../japanese/romaji_converter.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{
class Database
{
  public:
    explicit Database(const std::filesystem::path &path)
    {
        if (sqlite3_open(metasequoia::path_to_utf8(path).c_str(), &database_) != SQLITE_OK)
        {
            throw std::runtime_error("Failed to create the test dictionary.");
        }
    }

    ~Database()
    {
        sqlite3_close(database_);
    }

    void execute(const char *sql)
    {
        char *error = nullptr;
        if (sqlite3_exec(database_, sql, nullptr, nullptr, &error) != SQLITE_OK)
        {
            const std::string message = error == nullptr ? "SQLite operation failed." : error;
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }

  private:
    sqlite3 *database_ = nullptr;
};
} // namespace

int main()
{
    if (japanese::HiraganaToKatakana("かな") != "カナ" || japanese::HiraganaToRomaji("カナ") != "kana")
    {
        throw std::runtime_error("Kana conversion changed during the platform port.");
    }

    const auto unique_suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path data_directory = std::filesystem::temp_directory_path() / std::filesystem::u8path("metasequoia-engine-词库-" + unique_suffix);
    std::filesystem::create_directories(data_directory);
#ifdef _WIN32
    if (_wputenv_s(L"METASEQUOIA_IME_DATA_DIR", data_directory.c_str()) != 0)
#else
    if (setenv("METASEQUOIA_IME_DATA_DIR", metasequoia::path_to_utf8(data_directory).c_str(), 1) != 0)
#endif
    {
        throw std::runtime_error("Failed to set the data directory override.");
    }

    {
        Database database(data_directory / "msime.db");
        database.execute("CREATE TABLE tbl_2_n(key TEXT, jp TEXT, value TEXT, weight INTEGER)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '你好', 100)");

        ImeSession session(SchemeType::Quanpin);
        for (const char character : std::string("nihao"))
        {
            const ImeKeyCode key_code = static_cast<ImeKeyCode>(character - ('a' - 'A'));
            session.handle_key(key_code, 0, static_cast<ImeCharacter>(character));
        }

        if (session.get_preedit() != "nihao")
        {
            throw std::runtime_error("Quanpin preedit does not contain the typed input.");
        }
        const auto &candidates = session.get_candidates();
        if (std::none_of(candidates.begin(), candidates.end(), [](const WordItem &item) { return item.word == "你好"; }))
        {
            throw std::runtime_error("Quanpin did not return the candidate stored in the dictionary.");
        }

        session.handle_key(ImeKey::Backspace);
        if (session.get_preedit() != "niha")
        {
            throw std::runtime_error("Backspace did not update the quanpin preedit.");
        }
    }

    std::filesystem::remove_all(data_directory);
    return 0;
}
