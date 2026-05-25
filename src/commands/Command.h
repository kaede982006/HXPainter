#pragma once

#include "render/CanvasDocument.h"

#include <functional>
#include <QString>

class Command {
public:
    virtual ~Command() = default;
    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual bool isNoOp() const;
    virtual void undo() = 0;
    virtual void redo() = 0;
};

class DocumentSnapshotCommand final : public Command {
public:
    DocumentSnapshotCommand(QString name, CanvasDocument *document, DocumentState before, DocumentState after);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isNoOp() const override;
    void undo() override;
    void redo() override;

private:
    QString name_;
    CanvasDocument *document_ = nullptr;
    DocumentState before_;
    DocumentState after_;
};

class FunctionalCommand final : public Command {
public:
    FunctionalCommand(QString name, std::function<void()> undo, std::function<void()> redo);

    [[nodiscard]] QString name() const override;
    void undo() override;
    void redo() override;

private:
    QString name_;
    std::function<void()> undo_;
    std::function<void()> redo_;
};
