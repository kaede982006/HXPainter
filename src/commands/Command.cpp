#include "Command.h"

#include <cmath>

namespace {
bool sameLayer(const Layer &lhs, const Layer &rhs)
{
    return lhs.id == rhs.id
        && lhs.name == rhs.name
        && lhs.image == rhs.image
        && lhs.visible == rhs.visible
        && lhs.locked == rhs.locked
        && lhs.alphaLock == rhs.alphaLock
        && std::abs(lhs.opacity - rhs.opacity) <= 0.000001
        && lhs.blendMode == rhs.blendMode;
}

bool sameStateForUndo(const DocumentState &lhs, const DocumentState &rhs)
{
    if (lhs.canvasSize != rhs.canvasSize
        || lhs.dpi != rhs.dpi
        || lhs.backgroundColor != rhs.backgroundColor
        || lhs.transparentBackground != rhs.transparentBackground
        || lhs.colorSpace != rhs.colorSpace
        || lhs.bitDepth != rhs.bitDepth
        || lhs.activeLayerIndex != rhs.activeLayerIndex
        || lhs.filePath != rhs.filePath
        || lhs.layers.size() != rhs.layers.size()) {
        return false;
    }

    for (int i = 0; i < lhs.layers.size(); ++i) {
        if (!sameLayer(lhs.layers[i], rhs.layers[i])) {
            return false;
        }
    }
    return true;
}
}

bool Command::isNoOp() const
{
    return false;
}

DocumentSnapshotCommand::DocumentSnapshotCommand(QString name,
                                                 CanvasDocument *document,
                                                 DocumentState before,
                                                 DocumentState after)
    : name_(std::move(name))
    , document_(document)
    , before_(std::move(before))
    , after_(std::move(after))
{
}

QString DocumentSnapshotCommand::name() const
{
    return name_;
}

bool DocumentSnapshotCommand::isNoOp() const
{
    return sameStateForUndo(before_, after_);
}

void DocumentSnapshotCommand::undo()
{
    if (document_) {
        document_->restore(before_);
    }
}

void DocumentSnapshotCommand::redo()
{
    if (document_) {
        document_->restore(after_);
    }
}

FunctionalCommand::FunctionalCommand(QString name, std::function<void()> undo, std::function<void()> redo)
    : name_(std::move(name))
    , undo_(std::move(undo))
    , redo_(std::move(redo))
{
}

QString FunctionalCommand::name() const
{
    return name_;
}

void FunctionalCommand::undo()
{
    if (undo_) {
        undo_();
    }
}

void FunctionalCommand::redo()
{
    if (redo_) {
        redo_();
    }
}
