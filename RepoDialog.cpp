#include "RepoDialog.h"
#include "ui_RepoDialog.h"

RepoDialog::RepoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RepoDialog)
{
    ui->setupUi(this);
}

RepoDialog::~RepoDialog()
{
    delete ui;
}
