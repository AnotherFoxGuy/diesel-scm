#include "SettingsDialog.h"
#include "Utils.h"
#include "ui_SettingsDialog.h"

#include <QDir>

///////////////////////////////////////////////////////////////////////////////
SettingsDialog::SettingsDialog(QWidget *parent, Settings &_settings) : QDialog(parent, Qt::Sheet), ui(new Ui::SettingsDialog), settings(&_settings)
{
    ui->setupUi(this);

    CreateLangMap();

    ui->cmbDoubleClickAction->addItem(tr("Diff File"));
    ui->cmbDoubleClickAction->addItem(tr("Open File"));
    ui->cmbDoubleClickAction->addItem(tr("Open Containing Folder"));
    ui->cmbDoubleClickAction->addItem(tr("Custom Action %0").arg(1));

    ui->cmbFossilBrowser->addItem(tr("System"));
    ui->cmbFossilBrowser->addItem(tr("Internal"));

    // App Settings
    ui->lineFossilPath->setText(QDir::toNativeSeparators(settings->GetValue(FUEL_SETTING_FOSSIL_PATH).toString()));
    ui->cmbDoubleClickAction->setCurrentIndex(settings->GetValue(FUEL_SETTING_FILE_DBLCLICK).toInt());
    ui->cmbFossilBrowser->setCurrentIndex(settings->GetValue(FUEL_SETTING_WEB_BROWSER).toInt());

    // Initialize language combo
    foreach (const LangMap &m, langMap)
        ui->cmbActiveLanguage->addItem(m.name);

    QString lang = settings->GetValue(FUEL_SETTING_LANGUAGE).toString();
    // Select current language
    ui->cmbActiveLanguage->setCurrentIndex(ui->cmbActiveLanguage->findText(LangIdToName(lang)));

    lastActionIndex = 0;
    currentCustomActions = settings->GetCustomActions();

    ui->cmbCustomActionContext->addItem(tr("Files"));
    ui->cmbCustomActionContext->addItem(tr("Folders"));
    ui->cmbCustomActionContext->setCurrentIndex(0);

    GetCustomAction(0);

    for (auto &a : currentCustomActions)
    {
        ui->cmbCustomAction->addItem(a.Id);
    }
    ui->cmbCustomAction->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
SettingsDialog::~SettingsDialog()
{
    delete ui;
}

//-----------------------------------------------------------------------------
bool SettingsDialog::run(QWidget *parent, Settings &settings)
{
    SettingsDialog dlg(parent, settings);
    return dlg.exec() == QDialog::Accepted;
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_buttonBox_accepted()
{
    settings->SetValue(FUEL_SETTING_FOSSIL_PATH, QDir::fromNativeSeparators(ui->lineFossilPath->text()));
    Q_ASSERT(ui->cmbDoubleClickAction->currentIndex() >= FILE_DLBCLICK_ACTION_DIFF && ui->cmbDoubleClickAction->currentIndex() < FILE_DLBCLICK_ACTION_MAX);
    settings->SetValue(FUEL_SETTING_FILE_DBLCLICK, ui->cmbDoubleClickAction->currentIndex());
    settings->SetValue(FUEL_SETTING_WEB_BROWSER, ui->cmbFossilBrowser->currentIndex());

    Q_ASSERT(settings->HasValue(FUEL_SETTING_LANGUAGE));
    QString curr_langid = settings->GetValue(FUEL_SETTING_LANGUAGE).toString();
    QString new_langid = LangNameToId(ui->cmbActiveLanguage->currentText());
    Q_ASSERT(!new_langid.isEmpty());
    settings->SetValue(FUEL_SETTING_LANGUAGE, new_langid);

    if (curr_langid != new_langid)
        QMessageBox::information(this, tr("Restart required"), tr("The language change will take effect after restarting the application"), QMessageBox::Ok);

    for (auto &a : currentCustomActions)
    {
        a.Description = a.Description.trimmed();
        a.Command = a.Command.trimmed();
    }

    PutCustomAction(ui->cmbCustomAction->currentIndex());

    settings->GetCustomActions() = currentCustomActions;

    settings->ApplyEnvironment();
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectFossil_clicked()
{
    QString path = SelectExe(this, tr("Select Fossil executable"));
    if (!path.isEmpty())
        ui->lineFossilPath->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnClearMessageHistory_clicked()
{
    if (DialogQuery(this, tr("Clear Commit Message History"), tr("Are you sure you want to clear the commit message history?")) == QMessageBox::Yes)
        settings->SetValue(FUEL_SETTING_COMMIT_MSG, QStringList());
}

//-----------------------------------------------------------------------------
void SettingsDialog::CreateLangMap()
{
    langMap.append(LangMap("nl_NL", "Dutch (NL)"));
    langMap.append(LangMap("en_US", "English (US)"));
    langMap.append(LangMap("fr_FR", "French (FR)"));
    langMap.append(LangMap("de_DE", "German (DE)"));
    langMap.append(LangMap("el_GR", "Greek (GR)"));
    langMap.append(LangMap("it_IT", "Italian (IT)"));
    langMap.append(LangMap("ko_KR", "Korean (KO)"));
    langMap.append(LangMap("pt_PT", "Portuguese (PT)"));
    langMap.append(LangMap("ru_RU", "Russian (RU)"));
    langMap.append(LangMap("es_ES", "Spanish (ES)"));
}

//-----------------------------------------------------------------------------
QString SettingsDialog::LangIdToName(const QString &id)
{
    foreach (const LangMap &m, langMap)
    {
        if (m.id == id)
            return m.name;
    }

    return "";
}

//-----------------------------------------------------------------------------
QString SettingsDialog::LangNameToId(const QString &name)
{
    foreach (const LangMap &m, langMap)
    {
        if (m.name == name)
            return m.id;
    }

    return "";
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_btnSelectCustomFileActionCommand_clicked()
{
    QString path = SelectExe(this, tr("Select command"));
    if (path.isEmpty())
        return;

    // Quote path if it contains spaces
    if (path.indexOf(' ') != -1)
        path = '"' + path + '"';

    ui->lineCustomActionCommand->setText(QDir::toNativeSeparators(path));
}

//-----------------------------------------------------------------------------
void SettingsDialog::GetCustomAction(int index)
{
    Q_ASSERT(index >= 0 && index < currentCustomActions.size());
    CustomAction &action = currentCustomActions[index];
    ui->lineCustomActionDescription->setText(action.Description);
    ui->lineCustomActionCommand->setText(action.Command);
    ui->cmbCustomActionContext->setCurrentIndex(action.Context - 1);
    ui->chkCustomActionMultipleSelection->setChecked(action.MultipleSelection);
}

//-----------------------------------------------------------------------------
void SettingsDialog::PutCustomAction(int index)
{
    Q_ASSERT(index >= 0 && index < currentCustomActions.size());
    CustomAction &action = currentCustomActions[index];
    action.Description = ui->lineCustomActionDescription->text().trimmed();
    action.Command = ui->lineCustomActionCommand->text().trimmed();
    action.Context = static_cast<CustomActionContext>(ui->cmbCustomActionContext->currentIndex() + 1);
    action.MultipleSelection = ui->chkCustomActionMultipleSelection->isChecked();
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_cmbCustomAction_currentIndexChanged(int index)
{
    if (index != lastActionIndex)
        PutCustomAction(lastActionIndex);

    GetCustomAction(index);
    lastActionIndex = index;
}

//-----------------------------------------------------------------------------
void SettingsDialog::on_cmbCustomActionContext_currentIndexChanged(int index)
{
    int action_index = ui->cmbCustomAction->currentIndex();
    if (action_index < 0)
        return;
    Q_ASSERT(action_index >= 0 && action_index < currentCustomActions.size());
    currentCustomActions[action_index].Context = static_cast<CustomActionContext>(index + 1);
}
