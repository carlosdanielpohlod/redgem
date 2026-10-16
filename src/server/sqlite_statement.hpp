#pragma once

#include <string>

#include <sqlite3.h>

namespace redgem::server {

// One SQL statement on a database, finished when it goes. The texts bound to it are not copied:
// they must outlive its last step.
class SqliteStatement {
 public:
  SqliteStatement(sqlite3* database, const char* sql) { sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr); }
  ~SqliteStatement() { sqlite3_finalize(statement_); }
  SqliteStatement(const SqliteStatement&) = delete;
  SqliteStatement& operator=(const SqliteStatement&) = delete;

  // Gives the statement's `?<place>` a text.
  SqliteStatement& bind(int place, const std::string& text) {
    sqlite3_bind_text(statement_, place, text.data(), static_cast<int>(text.size()), SQLITE_STATIC);
    return *this;
  }
  // Runs it to its next row: true while there is one.
  bool next_row() { return sqlite3_step(statement_) == SQLITE_ROW; }
  // Runs it whole: true when it did all it says.
  bool run() { return sqlite3_step(statement_) == SQLITE_DONE; }
  // The text in a column of the row it is at.
  std::string text(int column) const {
    const unsigned char* text = sqlite3_column_text(statement_, column);
    const int bytes = sqlite3_column_bytes(statement_, column);
    return text != nullptr ? std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes)) : "";
  }

 private:
  sqlite3_stmt* statement_ = nullptr;
};

}  // namespace redgem::server
