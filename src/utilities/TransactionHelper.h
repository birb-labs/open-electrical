// =============================================================================
//  TransactionHelper.h - RAII helpers around BRX transactions and document
//  locking, plus the plugin's undo mark bookkeeping (EL-UNDO).
// =============================================================================
#pragma once

#include "Platform.h"

namespace electrical {

// Scoped document lock. Required when mutating the database from modeless UI
// (the wxWidgets palette) or from an application/session context command.
class DocumentLock {
public:
    DocumentLock();
    ~DocumentLock();
    bool locked() const { return locked_; }
private:
    bool locked_ = false;
#if defined(_ELECTRICAL_WIN) || defined(_ELECTRICAL_LINUX)
    AcApDocument* doc_ = nullptr;
#endif
};

// Scoped transaction on the working database. Commits on success unless abort()
// was called; aborts automatically if an exception unwinds through it.
class Transaction {
public:
    Transaction();
    ~Transaction();

#if defined(_ELECTRICAL_WIN)
    AcDbTransaction* raw() const { return tr_; }
#else
    AcTransaction* raw() const { return tr_; }
#endif
    void abort() { abort_ = true; }
    void commit();   // explicit early commit

private:
    AcTransactionManager* mgr_ = nullptr;
#if defined(_ELECTRICAL_WIN)
    AcDbTransaction*      tr_   = nullptr;
#else
    AcTransaction*        tr_   = nullptr;
#endif
    bool committed_ = false;
    bool abort_     = false;
};

// Marks the beginning of a logical plugin operation so EL-UNDO can roll back
// exactly one plugin action, independent of the user's own edits. Wraps the
// standard UNDO group markers.
class UndoGroup {
public:
    explicit UndoGroup(const ACHAR* label);
    ~UndoGroup();
};

// Rolls back the most recent plugin UndoGroup (implements EL-UNDO).
bool undoLastPluginAction();

} // namespace electrical
