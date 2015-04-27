#include "Bridge.h"
#include <QStringList>
#include <QCoreApplication>
#include <LoggedProcess.h>
#include <QTextCodec>
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QTemporaryFile>
#include "Utils.h"

static const unsigned char		UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };

// 19: [5c46757d4b9765] on 2012-04-22 04:41:15
static const QRegExp			REGEX_STASH("\\s*(\\d+):\\s+\\[(.*)\\] on (\\d+)-(\\d+)-(\\d+) (\\d+):(\\d+):(\\d+)", Qt::CaseInsensitive);

#define FOSSIL_CHECKOUT1	"_FOSSIL_"
#define FOSSIL_CHECKOUT2	".fslckout"
#define FOSSIL_EXT			"fossil"
#define PATH_SEP			"/"

//------------------------------------------------------------------------------
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
bool Bridge::openRepository(const QString& repositoryPath, const QString& workspacePath)
{
	QFileInfo fi(repositoryPath);

	if(!QDir::setCurrent(workspacePath) || !fi.isFile())
		return false;

	QString abspath = fi.absoluteFilePath();
	setCurrentWorkspace(workspacePath);
	setRepositoryFile(abspath);

	if(!runFossil(QStringList() << "open" << QuotePath(abspath)))
		return false;

	return true;
}

//------------------------------------------------------------------------------
bool Bridge::newRepository(const QString& repositoryPath)
{
	QFileInfo fi(repositoryPath);

	if(fi.exists())
		return false;

	if(!runFossil(QStringList() << "new" << QuotePath(fi.absoluteFilePath())))
		return false;
	return true;
}

//------------------------------------------------------------------------------
bool Bridge::closeRepository()
{
	if(!runFossil(QStringList() << "close"))
		return false;

	stopUI();
	setCurrentWorkspace("");
	return true;
}

//------------------------------------------------------------------------------
bool Bridge::listFiles(QStringList &files)
{
	return runFossil(QStringList() << "ls" << "-l", &files, RUNFLAGS_SILENT_ALL);
}

//------------------------------------------------------------------------------
bool Bridge::pushRepository()
{
	return runFossil(QStringList() << "push");
}

//------------------------------------------------------------------------------
bool Bridge::pullRepository()
{
	return runFossil(QStringList() << "pull");
}

//------------------------------------------------------------------------------
bool Bridge::cloneRepository(const QString& repository, const QUrl& url, const QUrl& proxyUrl)
{
	// Actual command
	QStringList cmd = QStringList() << "clone";

	// Log Command
	QStringList logcmd = QStringList() << "fossil" << "clone";

	QString source = url.toString();
	QString logsource = url.toString(QUrl::RemovePassword);
	if(url.isLocalFile())
	{
		source = url.toLocalFile();
		logsource = source;
	}
	cmd << source << repository;
	logcmd << logsource << repository;

	if(!proxyUrl.isEmpty())
	{
		cmd << "--proxy" << proxyUrl.toString();
		logcmd << "--proxy" << proxyUrl.toString(QUrl::RemovePassword);
	}

	log("<b>&gt;"+logcmd.join(" ")+"</b><br>", true);

	// Clone Repo
	if(!runFossil(cmd, 0, RUNFLAGS_SILENT_INPUT))
		return false;

	return true;
}

//------------------------------------------------------------------------------
bool Bridge::getFossilVersion(QString& version)
{
	QStringList res;
	if(!runFossil(QStringList() << "version", &res, RUNFLAGS_SILENT_ALL) && res.length()==1)
		return false;

	if(res.length()==0)
		return false;

	int off = res[0].indexOf("version ");
	if(off==-1)
		return false;

	version = res[0].mid(off+8);
	return true;
}

//------------------------------------------------------------------------------
bool Bridge::diffFile(const QString &repoFile)
{
	// Run the diff detached
	return runFossil(QStringList() << "gdiff" << QuotePath(repoFile), 0, RUNFLAGS_DETACHED);
}

//------------------------------------------------------------------------------
bool Bridge::commitFiles(const QStringList& fileList, const QString& comment)
{
	// Do commit
	QString comment_fname;
	{
		QTemporaryFile temp_file;
		if(!temp_file.open())
			return false;

		comment_fname = temp_file.fileName();
	}

	QFile comment_file(comment_fname);
	if(!comment_file.open(QIODevice::WriteOnly))
		return false;

	// Write BOM
	comment_file.write(reinterpret_cast<const char *>(UTF8_BOM), sizeof(UTF8_BOM));

	// Write Comment
	comment_file.write(comment.toUtf8());
	comment_file.close();

	// Generate fossil parameters.
	QStringList params;
	params << "commit" << "--message-file" << QuotePath(comment_fname);
	params << QuotePaths(fileList);

	runFossil(params);
	QFile::remove(comment_fname);
	return true;
}

//------------------------------------------------------------------------------
bool Bridge::addFiles(const QStringList& fileList)
{
	if(fileList.empty())
		return false;

	// Do Add
	return runFossil(QStringList() << "add" << QuotePaths(fileList));
}

//------------------------------------------------------------------------------
bool Bridge::removeFiles(const QStringList& fileList, bool deleteLocal)
{
	if(fileList.empty())
		return false;

	// Do Delete
	if(!runFossil(QStringList() << "delete" << QuotePaths(fileList)))
		return false;

	if(deleteLocal)
	{
		for(int i=0; i<fileList.size(); ++i)
		{
			QFileInfo fi(getCurrentWorkspace() + QDir::separator() + fileList[i]);
			if(fi.exists())
				QFile::remove(fi.filePath());
		}
	}

	return true;
}

//------------------------------------------------------------------------------
bool Bridge::revertFiles(const QStringList& fileList)
{
	if(fileList.empty())
		return false;

	// Do Revert
	return runFossil(QStringList() << "revert" << QuotePaths(fileList));
}

//------------------------------------------------------------------------------
bool Bridge::renameFile(const QString &beforePath, const QString &afterPath)
{
	// Ensure we can rename the file
	if(!QFileInfo(beforePath).exists() || QFileInfo(afterPath).exists())
		return false;

	// Do Rename
	if(!runFossil(QStringList() << "mv" << QuotePath(beforePath) << QuotePath(afterPath)))
		return false;

	QString wkdir = getCurrentWorkspace() + QDir::separator();

	// Also rename the file
	return QFile::rename(wkdir+beforePath, wkdir+afterPath);
}

//------------------------------------------------------------------------------
bool Bridge::undoRepository(QStringList &result, bool explainOnly)
{
	QStringList params;
	params << "undo";

	if(explainOnly)
		params << "--explain";

	result.clear();
	return runFossil(params, &result);
}

//------------------------------------------------------------------------------
bool Bridge::updateRepository(QStringList &result, bool explainOnly)
{
	QStringList params;
	params << "update";

	if(explainOnly)
		params << "--nochange";

	result.clear();
	return runFossil(params, &result);
}

//------------------------------------------------------------------------------
bool Bridge::stashList(stashmap_t& stashes)
{
	stashes.clear();
	QStringList res;

	if(!runFossil(QStringList() << "stash" << "ls", &res, RUNFLAGS_SILENT_ALL))
		return false;

	for(QStringList::iterator line_it=res.begin(); line_it!=res.end(); )
	{
		QString line = *line_it;

		int index = REGEX_STASH.indexIn(line);
		if(index==-1)
			break;

		QString id = REGEX_STASH.cap(1);
		++line_it;

		QString name;
		// Finish at an anonymous stash or start of a new stash ?
		if(line_it==res.end() || REGEX_STASH.indexIn(*line_it)!=-1)
			name = line.trimmed();
		else // Named stash
		{
			// Parse stash name
			name = (*line_it);
			name = name.trimmed();
			++line_it;
		}

		stashes.insert(name, id);
	}

	return true;
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


//------------------------------------------------------------------------------
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
