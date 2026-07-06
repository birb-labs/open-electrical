#include "utilities/TransactionHelper.h"

namespace electrical {

// ---------------------------------------------------------------------------
//  DocumentLock
// ---------------------------------------------------------------------------
DocumentLock::DocumentLock() {
    doc_ = acDocManager ? acDocManager->curDocument() : nullptr;
    if (doc_) {
        // Lock only if we are not already in the document context.
        if (acDocManager->lockDocument(doc_) == Acad::eOk)
            locked_ = true;
    }
}

DocumentLock::~DocumentLock() {
    if (locked_ && doc_)
        acDocManager->unlockDocument(doc_);
}

// ---------------------------------------------------------------------------
//  Transaction
// ---------------------------------------------------------------------------
Transaction::Transaction() {
    mgr_ = acdbHostApplicationServices()->workingDatabase()
             ? actrTransactionManager
             : nullptr;
    if (mgr_)
        tr_ = mgr_->startTransaction();
}

Transaction::~Transaction() {
    if (tr_ && !committed_) {
        if (abort_) mgr_->abortTransaction();
        else        mgr_->endTransaction();   // commit
    }
}

void Transaction::commit() {
    if (tr_ && !committed_) {
        mgr_->endTransaction();
        committed_ = true;
    }
}

// ---------------------------------------------------------------------------
//  Undo grouping - EL-UNDO peels back exactly one plugin action.
// ---------------------------------------------------------------------------
UndoGroup::UndoGroup(const ACHAR* /*label*/) {
    // BricsCAD groups editor operations between UNDO Begin/End markers. Issuing
    // them through the command mechanism keeps the plugin's actions atomic in
    // the standard undo stack.
    acDocManager->sendStringToExecute(
        acDocManager->curDocument(), _T("_.UNDO _Begin "), true, false, false);
}

UndoGroup::~UndoGroup() {
    acDocManager->sendStringToExecute(
        acDocManager->curDocument(), _T("_.UNDO _End "), true, false, false);
}

bool undoLastPluginAction() {
    // A single UNDO step rolls back the last group opened by UndoGroup.
    return acDocManager->sendStringToExecute(
               acDocManager->curDocument(), _T("_.UNDO 1 "),
               true, false, false) == Acad::eOk;
}

} // namespace electrical
