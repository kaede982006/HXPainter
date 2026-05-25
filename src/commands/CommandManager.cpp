#include "CommandManager.h"

CommandManager::CommandManager(QObject *parent)
    : QObject(parent)
{
}

void CommandManager::execute(std::unique_ptr<Command> command)
{
    if (!command || command->isNoOp()) {
        return;
    }
    const QString name = command->name();
    command->redo();
    undoStack_.push_back(std::move(command));
    redoStack_.clear();
    Q_EMIT commandApplied(name);
    Q_EMIT stackChanged();
}

void CommandManager::undo()
{
    if (undoStack_.empty()) {
        return;
    }
    std::unique_ptr<Command> command = std::move(undoStack_.back());
    undoStack_.pop_back();
    const QString name = QStringLiteral("Undo: %1").arg(command->name());
    command->undo();
    redoStack_.push_back(std::move(command));
    Q_EMIT commandApplied(name);
    Q_EMIT stackChanged();
}

void CommandManager::redo()
{
    if (redoStack_.empty()) {
        return;
    }
    std::unique_ptr<Command> command = std::move(redoStack_.back());
    redoStack_.pop_back();
    const QString name = QStringLiteral("Redo: %1").arg(command->name());
    command->redo();
    undoStack_.push_back(std::move(command));
    Q_EMIT commandApplied(name);
    Q_EMIT stackChanged();
}

void CommandManager::clear()
{
    undoStack_.clear();
    redoStack_.clear();
    Q_EMIT stackChanged();
}

bool CommandManager::canUndo() const
{
    return !undoStack_.empty();
}

bool CommandManager::canRedo() const
{
    return !redoStack_.empty();
}

QStringList CommandManager::undoHistory() const
{
    QStringList history;
    for (const auto &command : undoStack_) {
        history << command->name();
    }
    return history;
}
