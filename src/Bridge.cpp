#include "Bridge.h"
#include <QStringList>
#include <QCoreApplication>
#include <LoggedProcess.h>
#include <QTextCodec>
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QTemporaryFile>

static const unsigned char		UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };

#include "Utils.h"

Bridge::RepoStatus Bridge::getRepoStatus()
{
	QStringList res;
	int exit_code = EXIT_FAILURE;

	// We need to determine the reason why fossil has failed
	// so we delay processing of the exit_code
	if(!runFossilRaw(QStringList() << "info", &res, &exit_code, RUNFLAGS_SILENT_ALL))
		return REPO_NOT_FOUND;

	bool run_ok = exit_code == EXIT_SUCCESS;

	for(QStringList::iterator it=res.begin(); it!=res.end(); ++it)
	{
		int col_index = it->indexOf(':');
		if(col_index==-1)
			continue;

		QString key = it->left(col_index).trimmed();
		QString value = it->mid(col_index+1).trimmed();

		if(key=="fossil")
		{
			if(value=="incorrect repository schema version")
				return REPO_OLD_SCHEMA;
			else if(value=="not within an open checkout")
				return REPO_NOT_FOUND;
		}

		if(run_ok)
		{
			if(key=="project-name")
				projectName = value;
			else if(key=="repository")
				repositoryFile = value;
		}
	}

	return run_ok ? REPO_OK : REPO_NOT_FOUND;
}
//------------------------------------------------------------------------------
static QString ParseFossilQuery(QString line)
{
	// Extract question
	int qend = line.lastIndexOf('(');
	if(qend == -1)
		qend = line.lastIndexOf('[');
	Q_ASSERT(qend!=-1);
	line = line.left(qend);
	line = line.trimmed();
	line += "?";
	line[0]=QString(line[0]).toUpper()[0];
	return line;
}


//------------------------------------------------------------------------------
bool Bridge::runFossil(const QStringList &args, QStringList *output, int runFlags)
{
	int exit_code = EXIT_FAILURE;
	if(!runFossilRaw(args, output, &exit_code, runFlags))
		return false;

	return exit_code == EXIT_SUCCESS;
}


//------------------------------------------------------------------------------
// Run fossil. Returns true if execution was successful regardless if fossil
// issued an error
bool Bridge::runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, int runFlags)
{
	bool silent_input = (runFlags & RUNFLAGS_SILENT_INPUT) != 0;
	bool silent_output = (runFlags & RUNFLAGS_SILENT_OUTPUT) != 0;
	bool detached = (runFlags & RUNFLAGS_DETACHED) != 0;

	if(!silent_input)
	{
		QString params;
		foreach(QString p, args)
		{
			if(p.indexOf(' ')!=-1)
				params += '"' + p + "\" ";
			else
				params += p + ' ';
		}
		log("<b>&gt; fossil "+params+"</b><br>", true);
	}

	QString wkdir = getCurrentWorkspace();

	QString fossil = getFossilPath();

	// Detached processes use the command-line only, to avoid having to wait
	// for the temporary args file to be released before returing
	if(detached)
		return QProcess::startDetached(fossil, args, wkdir);

	// Generate args file
	const QStringList *final_args = &args;
	QTemporaryFile args_file;
	if(!args_file.open())
	{
		log(tr("Could not generate command line file"));
		return false;
	}

	// Write BOM
	args_file.write(reinterpret_cast<const char *>(UTF8_BOM), sizeof(UTF8_BOM));

	// Write Args
	foreach(const QString &arg, args)
	{
		args_file.write(arg.toUtf8());
		args_file.write("\n");
	}
	args_file.close();

	// Replace args with args filename
	QStringList run_args;
	run_args.append("--args");
	run_args.append(args_file.fileName());
	final_args = &run_args;

	// Create fossil process
	LoggedProcess process(parentWidget);
	process.setWorkingDirectory(wkdir);

	process.start(fossil, *final_args);
	if(!process.waitForStarted())
	{
		log(tr("Could not start Fossil executable '%0'").arg(fossil)+"\n");
		return false;
	}
	const QChar EOL_MARK('\n');
	QString ans_yes = 'y' + EOL_MARK;
	QString ans_no = 'n' + EOL_MARK;
	QString ans_always = 'a' + EOL_MARK;
	QString ans_convert = 'c' + EOL_MARK;

	abortOperation = false;
	QString buffer;

#ifdef Q_OS_WIN
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
#else
	QTextCodec *codec = QTextCodec::codecForLocale();
#endif

	Q_ASSERT(codec);
	QTextDecoder *decoder = codec->makeDecoder();
	Q_ASSERT(decoder);

	while(true)
	{
		QProcess::ProcessState state = process.state();
		qint64 bytes_avail = process.logBytesAvailable();

		if(state!=QProcess::Running && bytes_avail<1)
			break;

		if(abortOperation)
		{
			log("\n* "+tr("Terminated")+" *\n");
			#ifdef Q_OS_WIN		// Verify this is still true on Qt5
				process.kill(); // QT on windows cannot terminate console processes with QProcess::terminate
			#else
				process.terminate();
			#endif
			break;
		}

		QByteArray input;
		process.getLogAndClear(input);

		#ifdef QT_DEBUG // Log fossil output in debug builds
		if(!input.isEmpty())
			qDebug() << input;
		#endif

		buffer += decoder->toUnicode(input);

		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if(buffer.isEmpty())
			continue;

		// Normalize line endings
		buffer = buffer.replace("\r\n", "\n");
		buffer = buffer.replace("\r", "\n");

		// Extract the last line
		int last_line_start = buffer.lastIndexOf(EOL_MARK);

		QString last_line;
		QString before_last_line;
		if(last_line_start != -1)
		{
			last_line = buffer.mid(last_line_start+1); // Including the EOL

			// Detect previous line
			if(last_line_start>0)
			{
				int before_last_line_start = buffer.lastIndexOf(EOL_MARK, last_line_start-1);
				// No line before ?
				if(before_last_line_start==-1)
					before_last_line_start = 0; // Use entire line

				// Extract previous line
				before_last_line = buffer.mid(before_last_line_start, last_line_start-before_last_line_start);
			}
		}
		else
			last_line = buffer;

		last_line = last_line.trimmed();

		// Check if we have a query
		bool ends_qmark = !last_line.isEmpty() && last_line[last_line.length()-1]=='?';
		bool have_yn_query = last_line.toLower().indexOf("y/n")!=-1;
		bool have_yna_query = last_line.toLower().indexOf("a=always/y/n")!=-1 || last_line.toLower().indexOf("yes/no/all")!=-1 || last_line.toLower().indexOf("a=all/y/n")!=-1;
		bool have_an_query = last_line.toLower().indexOf("a=always/n")!=-1;
		bool have_acyn_query = last_line.toLower().indexOf("a=all/c=convert/y/n")!=-1;

		bool have_query = ends_qmark && (have_yn_query || have_yna_query || have_an_query || have_acyn_query);

		// Flush all complete lines to the log and output
		QStringList log_lines = buffer.left(last_line_start).split(EOL_MARK);
		for(int l=0; l<log_lines.length(); ++l)
		{
			// Do not output the last line if it not complete
			if(l==log_lines.length()-1 && buffer[buffer.length()-1] != EOL_MARK )
				continue;

			QString line = log_lines[l].trimmed();

			if(line.isEmpty())
				continue;

			if(output)
				output->append(line);

			if(!silent_output)
				log(line+"\n");
		}

		// Remove everything we processed (including the EOL)
		buffer = buffer.mid(last_line_start+1) ;

		// Now process any query
		if(have_query && (have_yna_query || have_acyn_query))
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButtons buttons = QMessageBox::YesToAll|QMessageBox::Yes|QMessageBox::No;

			// Add any extra text available to the query
			before_last_line = before_last_line.trimmed();
			if(!before_last_line.isEmpty())
				query = before_last_line + "\n" + query;

			// Map the Convert option to the Apply button
			if(have_acyn_query)
				buttons |= QMessageBox::Apply;

			QMessageBox::StandardButton res = DialogQuery(parentWidget, "Fossil", query, buttons);
			if(res==QMessageBox::Yes)
			{
				process.write(ans_yes.toLatin1());
				log("Y\n");
			}
			else if(res==QMessageBox::YesAll)
			{
				process.write(ans_always.toLatin1());
				log("A\n");
			}
			else if(res==QMessageBox::Apply)
			{
				process.write(ans_convert.toLatin1());
				log("C\n");
			}
			else
			{
				process.write(ans_no.toLatin1());
				log("N\n");
			}
			buffer.clear();
		}
		else if(have_query && have_yn_query)
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButton res = DialogQuery(parentWidget, "Fossil", query);

			if(res==QMessageBox::Yes)
			{
				process.write(ans_yes.toLatin1());
				log("Y\n");
			}
			else
			{
				process.write(ans_no.toLatin1());
				log("N\n");
			}

			buffer.clear();
		}
		else if(have_query && have_an_query)
		{
			log(last_line);
			QString query = ParseFossilQuery(last_line);
			QMessageBox::StandardButton res = DialogQuery(parentWidget, "Fossil", query, QMessageBox::YesToAll|QMessageBox::No);
			if(res==QMessageBox::YesAll)
			{
				process.write(ans_always.toLatin1());
				log("A\n");
			}
			else
			{
				process.write(ans_no.toLatin1());
				log("N\n");
			}
			buffer.clear();
		}
	}

	delete decoder;

	// Must be finished by now
	Q_ASSERT(process.state()==QProcess::NotRunning);

	QProcess::ExitStatus es = process.exitStatus();

	if(es!=QProcess::NormalExit)
		return false;

	if(exitCode)
		*exitCode = process.exitCode();

	return true;
}

QString Bridge::getFossilPath()
{
	// Use the user-specified fossil if available
	QString fossil_path = fossilPath;
	if(!fossil_path.isEmpty())
		return QDir::toNativeSeparators(fossil_path);

	QString fossil_exe = "fossil";
#ifdef Q_OS_WIN
	fossil_exe += ".exe";
#endif
	// Use our fossil if available
	QString fuel_fossil = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + QDir::separator() + fossil_exe);

	if(QFile::exists(fuel_fossil))
		return fuel_fossil;

	// Otherwise assume there is a "fossil" executable in the path
	return fossil_exe;
}


#define FOSSIL_CHECKOUT1	"_FOSSIL_"
#define FOSSIL_CHECKOUT2	".fslckout"
#define FOSSIL_EXT			"fossil"
#define PATH_SEP			"/"


bool Bridge::isWorkspace(const QString &path)
{
	if(path.length()==0)
		return false;

	QFileInfo fi(path);
	QString wkspace = path;
	wkspace = fi.absoluteDir().absolutePath();
	QString checkout_file1 = wkspace + PATH_SEP + FOSSIL_CHECKOUT1;
	QString checkout_file2 = wkspace + PATH_SEP + FOSSIL_CHECKOUT2;

	return (QFileInfo(checkout_file1).exists() || QFileInfo(checkout_file2).exists());
}

//------------------------------------------------------------------------------
bool Bridge::uiRunning() const
{
	return fossilUI.state() == QProcess::Running;
}

//------------------------------------------------------------------------------
bool Bridge::startUI(const QString &httpPort)
{
	if(uiRunning())
	{
		log(tr("Fossil UI is already running")+"\n");
		return true;
	}

	fossilUI.setParent(parentWidget);
	fossilUI.setProcessChannelMode(QProcess::MergedChannels);
	fossilUI.setWorkingDirectory(getCurrentWorkspace());

	log("<b>&gt; fossil ui</b><br>", true);
	log(tr("Starting Fossil browser UI. Please wait.")+"\n");
	QString fossil = getFossilPath();

	fossilUI.start(fossil, QStringList() << "server" << "--localauth" << "-P" << httpPort );

	if(!fossilUI.waitForStarted() || fossilUI.state()!=QProcess::Running)
	{
		log(tr("Could not start Fossil executable '%s'").arg(fossil)+"\n");
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------
void Bridge::stopUI()
{
	if(uiRunning())
	{
#ifdef Q_WS_WIN
		fossilUI.kill(); // QT on windows cannot terminate console processes with QProcess::terminate
#else
		fossilUI.terminate();
#endif
	}
	fossilUI.close();
}
