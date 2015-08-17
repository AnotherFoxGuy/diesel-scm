#include "Fossil.h"
#include <QStringList>
#include <QCoreApplication>
#include <QTextCodec>
#include <QDebug>
#include <QDir>
#include <QTemporaryFile>
#include <QUrl>
#include "Utils.h"

static const unsigned char		UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };

// 19: [5c46757d4b9765] on 2012-04-22 04:41:15
static const QRegExp			REGEX_STASH("\\s*(\\d+):\\s+\\[(.*)\\] on (\\d+)-(\\d+)-(\\d+) (\\d+):(\\d+):(\\d+)", Qt::CaseInsensitive);

// Listening for HTTP requests on TCP port 8081
static const QRegExp			REGEX_PORT(".*TCP port ([0-9]+)\\n", Qt::CaseSensitive);

///////////////////////////////////////////////////////////////////////////////
RepoStatus Fossil::getRepoStatus()
{
	QStringList res;
	int exit_code = EXIT_FAILURE;

	// We need to determine the reason why fossil has failed
	// so we delay processing of the exit_code
	if(!runFossilRaw(QStringList() << "info", &res, &exit_code, RUNFLAGS_SILENT_ALL))
		return REPO_NOT_FOUND;

	bool run_ok = exit_code == EXIT_SUCCESS;

	activeTags.clear();
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
			else if(key=="checkout")
			{
				// f2121dad5e4565f55ed9ef882484dd5934af565f 2015-04-26 17:27:39 UTC
				QStringList tokens = value.split(' ', QString::SkipEmptyParts);
				Q_ASSERT(tokens.length()>0);
				currentRevision = tokens[0].trimmed();
			}
			else if(key=="tags")
			{
				QStringList tokens = value.split(',', QString::SkipEmptyParts);
				foreach(const QString &tag, tokens)
					activeTags.append(tag);
				activeTags.sort();
			}
		}
	}

	return run_ok ? REPO_OK : REPO_NOT_FOUND;
}

//------------------------------------------------------------------------------
bool Fossil::openRepository(const QString& repositoryPath, const QString& workspacePath)
{
	QFileInfo fi(repositoryPath);

	if(!QDir::setCurrent(workspacePath) || !fi.isFile())
		return false;

	QString abspath = fi.absoluteFilePath();
	setWorkspacePath(workspacePath);
	setRepositoryFile(abspath);

	if(!runFossil(QStringList() << "open" << QuotePath(abspath)))
		return false;

	return true;
}

//------------------------------------------------------------------------------
bool Fossil::newRepository(const QString& repositoryPath)
{
	QFileInfo fi(repositoryPath);

	if(fi.exists())
		return false;

	if(!runFossil(QStringList() << "new" << QuotePath(fi.absoluteFilePath())))
		return false;
	return true;
}

//------------------------------------------------------------------------------
bool Fossil::closeRepository()
{
	if(!runFossil(QStringList() << "close"))
		return false;

	stopUI();
	setWorkspacePath("");
	return true;
}

//------------------------------------------------------------------------------
bool Fossil::listFiles(QStringList &files)
{
	return runFossil(QStringList() << "ls" << "-l", &files, RUNFLAGS_SILENT_ALL);
}

//------------------------------------------------------------------------------
bool Fossil::status(QStringList &result)
{
	return runFossil(QStringList() << "status", &result, RUNFLAGS_SILENT_ALL);
}

//------------------------------------------------------------------------------
bool Fossil::pushRepository(const QUrl &url)
{
	QStringList params;
	params << "push";

	int runFlags=RUNFLAGS_NONE;

	if(!url.isEmpty())
	{
		params << url.toEncoded();
		params << "--once";

		QStringList log_params = params;
		log_params[1] = UrlToStringDisplay(url);
		log_params.push_front("fossil");

		runFlags = RUNFLAGS_SILENT_INPUT;
		log("<b>&gt;"+log_params.join(" ")+"</b><br>", true);
	}

	return runFossil(params, 0, runFlags);
}

//------------------------------------------------------------------------------
bool Fossil::pullRepository(const QUrl &url)
{
	QStringList params;
	params << "pull";

	int runFlags=RUNFLAGS_NONE;

	if(!url.isEmpty())
	{
		params << url.toEncoded();
		params << "--once";

		QStringList log_params = params;
		log_params[1] = UrlToStringDisplay(url);

		log_params.push_front("fossil");

		runFlags = RUNFLAGS_SILENT_INPUT;
		log("<b>&gt;"+log_params.join(" ")+"</b><br>", true);
	}

	return runFossil(params, 0, runFlags);
}

//------------------------------------------------------------------------------
bool Fossil::cloneRepository(const QString& repository, const QUrl& url, const QUrl& proxyUrl)
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
bool Fossil::getFossilVersion(QString& version)
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
bool Fossil::diffFile(const QString &repoFile, bool graphical)
{
	if(graphical)
	{
		// Run the diff detached
		return runFossil(QStringList() << "gdiff" << QuotePath(repoFile), 0, RUNFLAGS_DETACHED);
	}
	else
		return runFossil(QStringList() << "diff" << QuotePath(repoFile));
}

//------------------------------------------------------------------------------
bool Fossil::commitFiles(const QStringList& fileList, const QString& comment, const QString &newBranchName, bool isPrivateBranch)
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

	// Commit to new branch
	if(!newBranchName.isEmpty())
	{
		params << "--branch" << newBranchName.trimmed();

		// Private branches are not synced with remotes
		if(isPrivateBranch)
			params << "--private";
	}

	params << QuotePaths(fileList);

	runFossil(params);
	QFile::remove(comment_fname);
	return true;
}

//------------------------------------------------------------------------------
bool Fossil::addFiles(const QStringList& fileList)
{
	if(fileList.empty())
		return false;

	// Do Add
	return runFossil(QStringList() << "add" << QuotePaths(fileList));
}

//------------------------------------------------------------------------------
bool Fossil::removeFiles(const QStringList& fileList, bool deleteLocal)
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
			QFileInfo fi(getWorkspacePath() + QDir::separator() + fileList[i]);
			if(fi.exists())
				QFile::remove(fi.filePath());
		}
	}

	return true;
}

//------------------------------------------------------------------------------
bool Fossil::revertFiles(const QStringList& fileList)
{
	if(fileList.empty())
		return false;

	// Do Revert
	return runFossil(QStringList() << "revert" << QuotePaths(fileList));
}

//------------------------------------------------------------------------------
bool Fossil::renameFile(const QString &beforePath, const QString &afterPath, bool renameLocal)
{
	// Ensure we can rename the file
	if(!QFileInfo(beforePath).exists() || QFileInfo(afterPath).exists())
		return false;

	// Do Rename
	if(!runFossil(QStringList() << "mv" << QuotePath(beforePath) << QuotePath(afterPath)))
		return false;

	QString wkdir = getWorkspacePath() + QDir::separator();

	// Also rename the file
	if(renameLocal && !QFile::rename(wkdir+beforePath, wkdir+afterPath))
		return false;

	return true;
}

//------------------------------------------------------------------------------
bool Fossil::undoRepository(QStringList &result, bool explainOnly)
{
	QStringList params;
	params << "undo";

	if(explainOnly)
		params << "--explain";

	result.clear();
	return runFossil(params, &result);
}

//------------------------------------------------------------------------------
bool Fossil::updateRepository(QStringList &result, const QString &revision, bool explainOnly)
{
	QStringList params;
	params << "update";

	if(explainOnly)
		params << "--nochange";

	if(revision.isEmpty())
		params << "--latest";
	else
		params << revision;


	result.clear();
	return runFossil(params, &result);
}

//------------------------------------------------------------------------------
bool Fossil::getFossilSettings(QStringList &result)
{
	return runFossil(QStringList() << "settings", &result, RUNFLAGS_SILENT_ALL);
}

//------------------------------------------------------------------------------
bool Fossil::setFossilSetting(const QString& name, const QString& value, bool global)
{
	QStringList params;

	if(value.isEmpty())
		params << "unset" << name;
	else
	{
		params << "settings" << name;

		// Quote when the value contains spaces
		if(value.indexOf(' ')!=-1)
			params << "\"" + value + "\"";
		else
			params << value;
	}

	if(global)
		params << "-global";

	return runFossil(params);
}

//------------------------------------------------------------------------------
bool Fossil::setRemoteUrl(const QUrl& url)
{
	QString u = url.toEncoded();

	if(url.isEmpty())
		u = "off";

	// FIXME: Fossil ignores any password passed via the URL
	// Run as silent to avoid displaying credentials in the log
	bool ok = runFossil(QStringList() << "remote-url" << u, 0, RUNFLAGS_SILENT_INPUT);

	return ok;
}

//------------------------------------------------------------------------------
bool Fossil::getRemoteUrl(QUrl& url)
{
	url.clear();

	QStringList out;
	if(!runFossil(QStringList() << "remote-url", &out, RUNFLAGS_SILENT_ALL))
		return false;

	QString url_str;
	if(out.length()>0)
		url_str = out[0].trimmed();

	if(url_str == "off")
		url.clear();
	else
		url.setUrl(url_str);

	return true;
}

//------------------------------------------------------------------------------
bool Fossil::stashNew(const QStringList& fileList, const QString& name, bool revert)
{
	// Do Stash
	// Snapshot just records the changes into the stash
	QString command = "snapshot";

	// Save also reverts the stashed files
	if(revert)
		command = "save";

	return runFossil(QStringList() << "stash" << command << "-m" << name << QuotePaths(fileList));
}

//------------------------------------------------------------------------------
bool Fossil::stashList(stashmap_t& stashes)
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
bool Fossil::stashApply(const QString& name)
{
	return runFossil(QStringList() << "stash" << "apply" << name);
}

//------------------------------------------------------------------------------
bool Fossil::stashDrop(const QString& name)
{
	return runFossil(QStringList() << "stash" << "drop" << name);
}

//------------------------------------------------------------------------------
bool Fossil::stashDiff(const QString& name)
{
	return runFossil(QStringList() << "stash" << "diff" << name, 0);
}

//------------------------------------------------------------------------------
bool Fossil::tagList(QStringMap& tags)
{
	tags.clear();
	QStringList tagnames;

	if(!runFossil(QStringList() << "tag" << "ls", &tagnames, RUNFLAGS_SILENT_ALL))
		return false;

	QStringList info;
	foreach(const QString &line, tagnames)
	{
		QString tag = line.trimmed();

		if(tag.isEmpty())
			continue;

		info.clear();

		// Use "whatis" instead of "info" to get extra information like the closed raw-tag
		if(!runFossil(QStringList() << "whatis" << "tag:"+tag, &info, RUNFLAGS_SILENT_ALL))
			return false;

		/*
		name:       tag:refactor
		artifact:   54059126aee6bb232373c1f134cc07ea0a6f4cca
		size:       15831 bytes
		tags:       refactor
		raw-tags:   closed
		type:       Check-in by kostas on 2015-04-30 19:23:15
		comment:    Renamed tableView to fileTableView Renamed treeView to workspaceTreeView Renamed
					tableViewStash to stashTableView
		*/

		QStringMap props;
		ParseProperties(props, info, ':');

		bool closed = false;
		if(props.contains("raw-tags"))
		{
			QString raw_tags = props["raw-tags"];
			closed = raw_tags.indexOf("closed") != -1;
		}

		// Skip closed tags, which are essentially closed branches.
		// FIXME: Fossil treating listing all closed branches as active tags with a "closed"
		// "raw-tag" seems weird. Maybe this will change at some point. Re-evaluate this
		// behaviour in the future.
		if(closed)
			continue;

		Q_ASSERT(props.contains("artifact"));
		QString revision = props["artifact"];
		Q_ASSERT(!revision.isEmpty());

		tags.insert(tag, revision);
	}
	return true;
}

//------------------------------------------------------------------------------
bool Fossil::tagNew(const QString& name, const QString& revision)
{
	QStringList res;

	if(!runFossil(QStringList() << "tag" << "add" << name << revision, &res))
		return false;
	return true;
}

//------------------------------------------------------------------------------
bool Fossil::tagDelete(const QString& name, const QString &revision)
{
	QStringList res;

	if(!runFossil(QStringList() << "tag" << "cancel" << "tag:"+name << revision, &res))
		return false;

	return true;
}

//------------------------------------------------------------------------------
bool Fossil::branchList(QStringList& branches, QStringList& activeBranches)
{
	branches.clear();
	activeBranches.clear();
	QStringList res;

	if(!runFossil(QStringList() << "branch" , &res, RUNFLAGS_SILENT_ALL))
		return false;

	foreach(const QString &line, res)
	{
		QString name = line.trimmed();

		if(name.isEmpty())
			continue;

		// Active branches start with a start
		int active_index = name.indexOf('*');
		bool is_active = (active_index != -1) && active_index==0;

		// Strip
		if(is_active)
		{
			name = name.mid(is_active+1).trimmed();
			activeBranches.append(name);
		}
		else
			branches.append(name);
	}

	branches.sort();
	activeBranches.sort();
	return true;
}

//------------------------------------------------------------------------------
bool Fossil::branchNew(const QString& name, const QString& revisionBasis, bool isPrivate)
{
	QStringList params;

	params <<"branch"  << "new" << name << revisionBasis;

	if(isPrivate)
		params << "--private";

	QStringList res;

	if(!runFossil(params, &res))
		return false;
	return true;
}

//------------------------------------------------------------------------------
bool Fossil::branchMerge(QStringList &res, const QString& revision, bool integrate, bool force, bool testOnly)
{
	QStringList params;

	params <<"merge";

	if(integrate)
		params << "--integrate";

	if(force)
		params << "--force";

	if(testOnly)
		params << "--dry-run";

	params << revision;

	if(!runFossil(params, &res))
		return false;
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
bool Fossil::runFossil(const QStringList &args, QStringList *output, int runFlags)
{
	int exit_code = EXIT_FAILURE;
	if(!runFossilRaw(args, output, &exit_code, runFlags))
		return false;

	return exit_code == EXIT_SUCCESS;
}

//------------------------------------------------------------------------------
// Run fossil. Returns true if execution was successful regardless if fossil
// issued an error
bool Fossil::runFossilRaw(const QStringList &args, QStringList *output, int *exitCode, int runFlags)
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

	QString wkdir = getWorkspacePath();

	QString fossil = getFossilPath();

	// Detached processes use the command-line only, to avoid having to wait
	// for the temporary args file to be released before returing
	if(detached)
	{
		bool started = QProcess::startDetached(fossil, args, wkdir);
		if(exitCode)
			*exitCode = started ? EXIT_SUCCESS : EXIT_FAILURE;
		return started;
	}

	// Make status message
	QString status_msg = QObject::tr("Running Fossil");
	if(args.length() > 0)
		status_msg = QString("Fossil %0").arg(args[0].toCaseFolded());
	ScopedStatus status(uiCallback, status_msg);

	// Generate args file
	const QStringList *final_args = &args;
	QTemporaryFile args_file;
	if(!args_file.open())
	{
		log(QObject::tr("Could not generate command line file"));
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
	// FIXME: when we are sure this works delete this
	// LoggedProcess process(parentWidget*/);
	LoggedProcess process(0);

	process.setWorkingDirectory(wkdir);

	process.start(fossil, *final_args);
	if(!process.waitForStarted())
	{
		log(QObject::tr("Could not start Fossil executable '%0'").arg(fossil)+"\n");
		return false;
	}
	const QChar EOL_MARK('\n');
	QString ans_yes = 'y' + EOL_MARK;
	QString ans_no = 'n' + EOL_MARK;
	QString ans_always = 'a' + EOL_MARK;
	QString ans_convert = 'c' + EOL_MARK;

	operationAborted = false;
	QString buffer;

#ifdef Q_OS_WIN
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
#else
	QTextCodec *codec = QTextCodec::codecForLocale();
#endif

	Q_ASSERT(codec);
	QTextDecoder *decoder = codec->makeDecoder();
	Q_ASSERT(decoder);

	Q_ASSERT(uiCallback);
#ifdef QT_DEBUG
	size_t input_index = 0;
#endif

	while(true)
	{
		QProcess::ProcessState state = process.state();
		qint64 bytes_avail = process.logBytesAvailable();

		if(state!=QProcess::Running && bytes_avail<1)
			break;

		if(operationAborted)
		{
			log("\n* "+QObject::tr("Terminated")+" *\n");
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
		if(!input.isEmpty() && (runFlags & RUNFLAGS_DEBUG) )
			qDebug() << "[" << ++input_index << "] '" << input.data() << "'\n";
		#endif

		buffer += decoder->toUnicode(input);
#if 0 // Keep this for now to simulate bad parses
		if(runFlags & RUNFLAGS_DEBUG)
		{
			static int debug=1;
			if(debug==1)
			{
				buffer = "     4: [df2233aabe09ef] on 2013-02-03 02:51:33\n"
						 "     History\n"
						 "     5: [df2233aabe09ef] on 2013-02-15 03:55:37\n"
						 "     ";
				debug++;
			}
			else if(debug==2)
			{
				buffer = " Diff\n"
						 " ";
				debug++;
			}
			else
				buffer="";
		}
#endif
		#ifdef QT_DEBUG // breakpoint
		//if(buffer.indexOf("SQLITE_CANTOPEN")!=-1)
		//	qDebug() << "Breakpoint\n";
		#endif

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

				// No new-line before ?
				if(before_last_line_start==-1)
					before_last_line_start = 0; // Use entire line
				else
					++before_last_line_start; // Skip new-line

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
			// Skip last line if we have a query. This will be handled manually further down
			if(have_query && l==log_lines.length()-1)
				continue;

			QString line = log_lines[l].trimmed();

		#ifdef QT_DEBUG
			if(runFlags & RUNFLAGS_DEBUG)
			{
				qDebug() << "LINE: " << line << "\n";
			}
		#endif

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

			QMessageBox::StandardButton res = uiCallback->Query("Fossil", query, buttons);
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
			QMessageBox::StandardButton res = uiCallback->Query("Fossil", query, QMessageBox::Yes|QMessageBox::No);

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
			QMessageBox::StandardButton res = uiCallback->Query("Fossil", query, QMessageBox::YesToAll|QMessageBox::No);
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

//------------------------------------------------------------------------------
QString Fossil::getFossilPath()
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
bool Fossil::isWorkspace(const QString &path)
{
	if(path.length()==0)
		return false;

	QFileInfo fi(path);
	QString wkspace = path;
	wkspace = fi.absoluteDir().absolutePath();
	QString checkout_file1 = wkspace + PATH_SEPARATOR + FOSSIL_CHECKOUT1;
	QString checkout_file2 = wkspace + PATH_SEPARATOR + FOSSIL_CHECKOUT2;

	return (QFileInfo(checkout_file1).exists() || QFileInfo(checkout_file2).exists());
}

//------------------------------------------------------------------------------
bool Fossil::uiRunning() const
{
	return fossilUI.state() == QProcess::Running;
}

//------------------------------------------------------------------------------
bool Fossil::startUI(const QString &httpPort)
{
	if(uiRunning())
	{
		log(QObject::tr("Fossil UI is already running")+"\n");
		return true;
	}

	// FIXME: when we are sure this works delete this
	//fossilUI.setParent(parentWidget);

	fossilUI.setProcessChannelMode(QProcess::MergedChannels);
	fossilUI.setWorkingDirectory(getWorkspacePath());

	log("<b>&gt; fossil ui</b><br>", true);
	log(QObject::tr("Starting Fossil browser UI. Please wait.")+"\n");
	QString fossil = getFossilPath();

	QStringList params;
	params << "server" << "--localauth";

	if(!httpPort.isEmpty())
		params << "-P" << httpPort;

	fossilUI.start(getFossilPath(), params);

	if(!fossilUI.waitForStarted() || fossilUI.state()!=QProcess::Running)
	{
		log(QObject::tr("Could not start Fossil executable '%s'").arg(fossil)+"\n");
		return false;
	}

#ifdef Q_OS_WIN
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
#else
	QTextCodec *codec = QTextCodec::codecForLocale();
#endif

	Q_ASSERT(codec);
	QTextDecoder *decoder = codec->makeDecoder();
	Q_ASSERT(decoder);

	fossilUIPort.clear();

	// Wait for fossil to report the http port
	QString buffer;
	while(true)
	{
		QProcess::ProcessState state = fossilUI.state();
		qint64 bytes_avail = fossilUI.logBytesAvailable();

		if(state!=QProcess::Running && bytes_avail<1)
			break;

		QByteArray input;
		fossilUI.getLogAndClear(input);

		buffer += decoder->toUnicode(input);

		// Normalize line endings
		buffer = buffer.replace("\r\n", "\n");
		buffer = buffer.replace("\r", "\n");

		int index = REGEX_PORT.indexIn(buffer);
		if(index==-1)
		{
			QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			continue;
		}

		// Extract port
		fossilUIPort = REGEX_PORT.cap(1).trimmed();

		// Done parsing
		break;
	}
	return true;
}

//------------------------------------------------------------------------------
void Fossil::stopUI()
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
	fossilUIPort.clear();
}

//------------------------------------------------------------------------------
QString Fossil::getUIHttpAddress() const
{
	if(fossilUIPort.isEmpty())
		return QString();
	return "http://127.0.0.1:"+fossilUIPort;
}
