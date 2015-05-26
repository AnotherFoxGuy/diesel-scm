#include "CommitDialog.h"
#include <QPushButton>
#include <QShortcut>
#include "ui_CommitDialog.h"
#include "MainWindow.h" // Ugly. I know.

CommitDialog::CommitDialog(QWidget *parent, const QString &title, QStringList &files, const QStringList *history, bool stashMode) :
	QDialog(parent, Qt::Sheet),
	ui(new Ui::CommitDialog)
{
	ui->setupUi(this);
	ui->plainTextEdit->clear();
	ui->listView->setModel(&itemModel);

	setWindowTitle(title);

	// Activate the appropriate control based on mode
	ui->plainTextEdit->setVisible(!stashMode);
	ui->lineEdit->setVisible(stashMode);

	// Activate the checkbox if we have some text
	ui->chkRevertFiles->setVisible(stashMode);

	ui->widgetBranchOptions->setVisible(!stashMode);

	// Activate the combo if we have history
	ui->comboBox->setVisible(history!=0);
	if(history)
	{
		// Generate the history combo
		foreach(const QString msg, *history)
		{
			QString trimmed = msg.trimmed();
			if(trimmed.isEmpty())
				continue;

			commitMessages.append(trimmed);
			QStringList lines = trimmed.split('\n');
			QString first_line;
			if(!lines.empty())
				first_line = lines[0] + "...";

			ui->comboBox->addItem(first_line);
		}
	}

	// Populate file list
	for(QStringList::const_iterator it=files.begin(); it!=files.end(); ++it)
	{
		QStandardItem *si = new QStandardItem(*it);
		si->setCheckable(true);
		si->setCheckState(Qt::Checked);
		itemModel.appendRow(si);
	}


	// Trigger commit with a Ctrl-Return from the comment box
	QAction* action = new QAction(ui->plainTextEdit);
	QShortcut* shortcut = new QShortcut(QKeySequence("Ctrl+Return"), ui->plainTextEdit);
	action->setAutoRepeat(false);
	connect(shortcut, SIGNAL(activated()), ui->buttonBox->button(QDialogButtonBox::Ok), SLOT(click()));

	// Abort commit with an Escape key from the comment box
	action = new QAction(ui->plainTextEdit);
	shortcut = new QShortcut(QKeySequence("Escape"), ui->plainTextEdit);
	action->setAutoRepeat(false);
	connect(shortcut, SIGNAL(activated()), ui->buttonBox->button(QDialogButtonBox::Cancel), SLOT(click()));
}

//------------------------------------------------------------------------------
CommitDialog::~CommitDialog()
{
	delete ui;
}

//------------------------------------------------------------------------------
bool CommitDialog::runCommit(QWidget* parent, QStringList& files, QString& commitMsg, const QStringList& commitMsgHistory, QString &branchName, bool &privateBranch)
{
	CommitDialog dlg(parent, tr("Commit Changes"), files, &commitMsgHistory, false);
	int res = dlg.exec();

	commitMsg = dlg.ui->plainTextEdit->toPlainText();

	if(res!=QDialog::Accepted)
		return false;

	files.clear();
	for(int i=0; i<dlg.itemModel.rowCount(); ++i)
	{
		QStandardItem *si = dlg.itemModel.item(i);
		if(si->checkState()!=Qt::Checked)
			continue;
		files.append(si->text());
	}

	branchName.clear();
	if(dlg.ui->chkNewBranch->isChecked())
	{
		branchName = dlg.ui->lineBranchName->text().trimmed();
		privateBranch = dlg.ui->chkPrivateBranch->isChecked();
	}

	return true;
}

//------------------------------------------------------------------------------
bool CommitDialog::runStashNew(QWidget* parent, QStringList& stashedFiles, QString& stashName, bool& revertFiles)
{
	CommitDialog dlg(parent, tr("Stash Changes"), stashedFiles, NULL, true);
	int res = dlg.exec();

	stashName = dlg.ui->lineEdit->text();

	if(res!=QDialog::Accepted)
		return false;

	stashedFiles.clear();
	for(int i=0; i<dlg.itemModel.rowCount(); ++i)
	{
		QStandardItem *si = dlg.itemModel.item(i);
		if(si->checkState()!=Qt::Checked)
			continue;
		stashedFiles.append(si->text());
	}

	revertFiles = dlg.ui->chkRevertFiles->checkState() == Qt::Checked;
	return true;
}

//------------------------------------------------------------------------------
void CommitDialog::on_comboBox_activated(int index)
{
	Q_ASSERT(index < commitMessages.length());

	if(ui->plainTextEdit->isVisible())
		ui->plainTextEdit->setPlainText(commitMessages[index]);

	if(ui->lineEdit->isVisible())
		ui->lineEdit->setText(commitMessages[index]);
}

//------------------------------------------------------------------------------
void CommitDialog::on_listView_doubleClicked(const QModelIndex &index)
{
	QVariant data = itemModel.data(index);
	QString filename = data.toString();
	reinterpret_cast<MainWindow*>(parent())->diffFile(filename);
}

//------------------------------------------------------------------------------
void CommitDialog::on_listView_clicked(const QModelIndex &)
{
	int num_selected = 0;
	for(int i=0; i<itemModel.rowCount(); ++i)
	{
		QStandardItem *si = itemModel.item(i);
		if(si->checkState()==Qt::Checked)
			++num_selected;
	}

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(num_selected>0);
}

//------------------------------------------------------------------------------
void CommitDialog::on_chkNewBranch_toggled(bool checked)
{
	ui->chkPrivateBranch->setEnabled(checked);
	ui->lineBranchName->setEnabled(checked);
}
