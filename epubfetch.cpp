// epubfetch.cpp : Defines the entry point for the console application.
// Output epub in application/xhtml+xml or text formats. The percentage
// out is provided by 3 X percent in hex without 0x at start.


#include "cgicc/CgiDefs.h"
#include "cgicc/Cgicc.h"
#include "cgicc/HTTPHTMLHeader.h"
#include "cgicc/HTMLClasses.h"

#if defined(WIN32)
typedef unsigned __int64 uint64_t; 
#else
#include <inttypes.h> // for uint64_t
#endif

#if HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif

#if HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

// MySql headers
#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
// boost
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include "boost/algorithm/string/replace.hpp"
#include <boost/scoped_ptr.hpp>
#include <boost/lexical_cast.hpp>

#if defined(WIN32)
#define PATH_SEPARATOR  "\\"
#include "Windows.h"
#include <direct.h>
#define GetCurrentDir _getcwd 
#else
#define PATH_SEPARATOR  "/"
#include <unistd.h> 
#define GetCurrentDir getcwd 
#endif

#define BUFSIZE 1024

//////////////////////////////
//Required namespace section//
//////////////////////////////
using namespace cgicc;
using namespace std;
using namespace boost::filesystem;
using namespace boost::algorithm;
using namespace boost::gregorian;
using namespace boost::posix_time;
using namespace boost::local_time;
//////////////////////////////////
//Forward declarations are here.//
//////////////////////////////////
void LogError(const Cgicc& cgi, string lFile, string pMsg);
string cleancodes(string& szcontent);
string stripHead(string& sznewpage);
string stripTail(string& sznewpage);
string cleanFile(string& sznewpage);
bool checkFileName(const string& filename);
/////////////////////////
//  Main Street, USA.  //
/////////////////////////
int
main(int /*argc*/, 
     char ** /*argv*/)
{
	string getServer = "https://diner.harpercollins.com/cgi-bin/epubFetch?img_id=";
//	string getServer = "http://10.40.85.20/cgi-bin/epubFetch?img_id=";

    bool m_isIsbn;
	try
	{
#ifndef WIN32
		setreuid(99, 99);  // To Safely start from rc5.d RedHat style
#endif
		Cgicc cgi;
	//	const CgiEnvironment& env = cgi.getEnvironment();
		std::string m_image; // the id for record
		std::string m_format; // the display format to use (text only).
		std::string m_isbn; // the isbn for record
		std::string m_href; // enable href code
		unsigned long m_len, m_multiLen, m_metaLen, m_id, m_pagewords;
		unsigned long m_allwords, m_maxpage;
		uint64_t m_edition_id;
		std::string m_mimetype, m_filename;
		std::string dbserver, dbuser, dbpass, dbstore, m_log, m_error;
		std::ostringstream m_strstrm;
		double m_pubpercent;
		double m_percent;
		std::string m_inpercent; 
		bool m_isSingle;

		const_form_iterator isbnIter = cgi.getElement("isbn");
		if (isbnIter != (*cgi).end() && !isbnIter->isEmpty())
		{
			m_isbn = (**isbnIter).c_str();
			m_isIsbn = true;
		} 
		else
		{
			m_error = "isbn for API Call invalid or missing";
			m_isIsbn = false;
		}

		const_form_iterator imageIter = cgi.getElement("img_id");
		if (imageIter != (*cgi).end() && !imageIter->isEmpty())
		{
			m_image = (**imageIter).c_str();
			m_isIsbn = false;
			m_id = boost::lexical_cast<unsigned long>(m_image.c_str());
		} 
		else
		{
			m_id = 666;
			m_error = "Sorry, we have not loaded this book into the system as yet.";
		}

		const_form_iterator formatIter = cgi.getElement("img_fmt");
		if (formatIter != (*cgi).end() && !formatIter->isEmpty())
		{
			m_format = (**formatIter).c_str();
		} 
		else
		{
			m_format = "full";
			m_error = "format API Call invalid or missing";
		}

		const_form_iterator hrefIter = cgi.getElement("href");
		if (hrefIter != (*cgi).end() && !hrefIter->isEmpty())
		{
			m_href = (**formatIter).c_str();
		} 
		else
		{
			m_href = "off";
		}

		const_form_iterator percentIter = cgi.getElement("developer_class");
		if (percentIter != (*cgi).end() && !percentIter->isEmpty())
		{
			std::string devClass = (**percentIter).c_str();
			if (devClass.compare("gold") == 0)
			{
				m_inpercent = "12C";
			}
			else
			{
				m_inpercent = "3C";
			}
//			m_inpercent = (**percentIter).c_str();
		} 
		else
		{	// default is 20% (3C = 60)
			m_inpercent = "3C";
		}

		const_form_iterator singleIter = cgi.getElement("single");
		if (singleIter != (*cgi).end() && !singleIter->isEmpty())
		{
			m_isSingle = true;
		} 
		else
		{	// first chapter only flag
			m_isSingle = false;
		}


#ifdef _DEBUG
		//		cgi.save("epubfetch.slp");	// no param test
		cgi.restore("D:\\epubfetch.slp"); // test
		m_isbn = "9780061874802";
		m_isIsbn = true;
		m_inpercent = "12C";
		//		m_id = 168;
		//		m_image = "168";
		//		m_isIsbn = false;
#endif

		// conversion of hex percentage is here
		{
			unsigned int perc;
			sscanf(m_inpercent.c_str(), "%x", &perc);
			m_percent = static_cast<double>(perc) / 100;
			m_percent = m_percent / 3;
		}
		if (m_percent > 1)
		{
			m_percent = 1;
		}

	#if defined(WIN32) || defined (_WIN64)
//		dbserver = "tcp://10.40.85.20:3306";
		dbserver = "tcp://ec2-184-72-94-122.compute-1.amazonaws.com:3306";
	#else
		dbserver = "tcp://127.0.0.1:3306";
	#endif
	
		dbuser = "ereader";
		dbpass = "testmybooks";
		dbstore = "ereader";
		m_metaLen = 0;
		m_multiLen = 0;
		m_pagewords = 0;
		m_allwords = 0;
		m_pubpercent = 0;
		m_maxpage = 0;
		m_edition_id = 0;

		sql::Driver *driver;
		sql::Connection *conn;
		sql::ResultSet *res;
	
		try
		{
			driver = get_driver_instance();
			conn = driver->connect(dbserver.c_str(), dbuser.c_str(), dbpass.c_str());
			conn->setSchema(dbstore.c_str());
		}
		catch (sql::SQLException x)
		{
			m_strstrm << x.what() << " connection";
			m_error = m_strstrm.str();
			m_strstrm.str("");
			cout << m_error;
			LogError(cgi, "/tmp/apache.err", m_error);
			return 666;
		}
		////////////////////////////////////////////////////
		//NOTE: Only used for query when ISBN is provided.//
		////////////////////////////////////////////////////
        if (m_isIsbn)
        {
            uint64_t idf = boost::lexical_cast<uint64_t>(m_isbn.c_str());
			m_edition_id = idf;
            if (idf == 0)
            {
                m_strstrm << "Sorry, we have not loaded : " << m_isbn << " into the system as yet.";
                m_error = m_strstrm.str();
                m_strstrm.str("");
                LogError(cgi, "/tmp/apache.err", m_error);
            }
            boost::scoped_ptr<sql::PreparedStatement> idstmt(conn->prepareStatement("SELECT id AS id, wordcount from ereader.item2 WHERE isfirstpage = 1 AND eid = ?"));
            try
            {
                idstmt->setInt64(1, idf);
            }
            catch (sql::SQLException x)
            {
                m_strstrm << x.what() << " prepareStatement";
                m_error = m_strstrm.str();
                m_strstrm.str("");
                LogError(cgi, "/tmp/apache.err", m_error);
            }
            bool noId = true;
            res = idstmt->executeQuery();
            try
            {
                while (res->next())
                {
                    m_id = res->getUInt("id");
					m_pagewords = res->getUInt("wordcount");
                    noId = false;
                }
                m_strstrm << m_id;
                m_image = m_strstrm.str();
                m_strstrm.str("");
            }
            catch (sql::SQLException x)
            {
                m_strstrm << x.what() << " results";
                m_error = m_strstrm.str();
                m_strstrm.str("");
                LogError(cgi, "/tmp/apache.err", m_error);
                std::ofstream eout("/tmp/slap.txt");
                m_strstrm << "suint64_t is " << idf;
                m_error = m_strstrm.str();
                eout << m_error;
                m_strstrm.str("");
            }

            if (noId)
            {
//				m_error = "Sorry, we have not loaded this book into the system as yet.";
               m_strstrm << "Sorry, we have not loaded this book into the system as yet.<br>" << endl;
               m_strstrm << "We are loading books on a regular schedule, so please check back." << endl;
               m_error = m_strstrm.str();
               m_strstrm.str("");
                LogError(cgi, "/tmp/apache.err", m_error);
                return EXIT_SUCCESS;
            }
            ///////////////////////////////////////////////////////////////
            // The delete res is REQUIRED to close the prepared statment.//
            ///////////////////////////////////////////////////////////////
			delete res;
			idstmt->close();
			idstmt.reset();
			boost::scoped_ptr<sql::PreparedStatement> wcstmt(conn->prepareStatement("SELECT SUM(wordcount) as total, MAX(isfirstpage) AS maxPage from item2 WHERE eid = ?"));
			try
			{
				wcstmt->setInt64(1, idf);
			}
			catch (sql::SQLException x)
			{
				m_strstrm << x.what() << " prepareStatement";
				m_error = m_strstrm.str();
				m_strstrm.str("");
				LogError(cgi, "/tmp/apache.err", m_error);
			}
			res = wcstmt->executeQuery();
			try
			{
				while (res->next())
				{
					m_allwords = res->getUInt("total");
					m_maxpage = res->getUInt("maxPage");
				}
			}
			catch (sql::SQLException x)
			{
				m_strstrm << x.what() << " results";
				m_error = m_strstrm.str();
				m_strstrm.str("");
				LogError(cgi, "/tmp/apache.err", m_error);
				std::ofstream eout("/tmp/slap.txt");
				m_strstrm << "suint64_t is " << idf;
				m_error = m_strstrm.str();
				eout << m_error;
				m_strstrm.str("");
			}
			delete res;
			wcstmt->close();
			wcstmt.reset();
        } // ISBN call end

		//////////////////////////////////
		//NOTE: End of ISBN only section//
		//////////////////////////////////
		boost::scoped_ptr<sql::PreparedStatement> stmt(conn->prepareStatement("SELECT e2.edition_id, e2.filename, i2.mediatype, i2.uncLength from epub2 e2, item2 i2 WHERE e2.id = i2.id and e2.id = ?"));
		try
		{
			if (m_isIsbn)
			{
				stmt->setUInt(1, m_id);
			}
			else
			{
				stmt->setUInt(1, atol(m_image.c_str()));
			}
		}
		catch (sql::SQLException x)
		{
			m_strstrm << x.what() << " prepareStatement";
			m_error = m_strstrm.str();
			m_strstrm.str("");
			LogError(cgi, "/tmp/apache.err", m_error);
		}
		res = stmt->executeQuery();
		bool idFound = false;
		try
		{
			while (res->next())
			{
				m_edition_id = res->getInt64("edition_id");
				m_filename = res->getString("filename").c_str();
				m_mimetype = res->getString("mediatype").c_str();
				m_len = res->getInt("uncLength");
				idFound = true;
			}
		}
		catch (sql::SQLException x)
		{
			m_strstrm << x.what() << " results";
			m_error = m_strstrm.str();
			m_strstrm.str("");
			LogError(cgi, "/tmp/apache.err", m_error);
		}
		delete res;
		stmt->close();
		stmt.reset();
		// -------------------------------------------------------------------------------
		//  Check we have received data for this ID	 --- 
		// -------------------------------------------------------------------------------	
		if(!idFound)
		{
			m_strstrm << "No information for ISBN : " << m_isbn;
			m_error = m_strstrm.str();
			m_strstrm.str("");
			LogError(cgi, "/tmp/apache.err", m_error);
			return EXIT_SUCCESS;
		}
		// , 
		boost::scoped_ptr<sql::PreparedStatement> qstmt(conn->prepareStatement("SELECT length(meta), length(multi) from epub2 WHERE id = ?"));
		try
		{
			if (m_isIsbn)
			{
				qstmt->setUInt(1, m_id);
			}
			else
			{
				qstmt->setUInt(1, atol(m_image.c_str()));
			}
		}
		catch (sql::SQLException x)
		{
			m_strstrm << x.what() << " prepareStatement";
			m_error = m_strstrm.str();
			m_strstrm.str("");
	
		}
		res = qstmt->executeQuery();
		try
		{
			while (res->next())
			{
				m_metaLen = res->getInt("length(meta)");
				m_multiLen = res->getInt("length(multi)");
			}
		}
		catch (sql::SQLException x)
		{
			m_strstrm << x.what() << " results";
			m_error = m_strstrm.str();
			m_strstrm.str("");
			cout << HTTPContentHeader("text/html");
			LogError(cgi, "/tmp/apache.err", m_error);
			return EXIT_SUCCESS;
		}
		delete res;
		qstmt->close();
		qstmt.reset();
		// -------------------------------------------------------------------------------
		//  NOTE: This is a text request for display. --- 
		// -------------------------------------------------------------------------------		
		if (m_metaLen > 0)
		{
			boost::scoped_ptr<sql::PreparedStatement> bstmt(conn->prepareStatement("SELECT meta from epub2 WHERE id = ?"));
			try
			{
				if (m_isIsbn)
				{
					bstmt->setUInt(1, m_id);
				}
				else
				{
					bstmt->setUInt(1, atol(m_image.c_str()));
				}
			}
			catch (sql::SQLException x)
			{
				m_strstrm << x.what() << " in cblob Statement";
				m_error = m_strstrm.str();
				m_strstrm.str("");
				cout << HTTPContentHeader("text/html");
				LogError(cgi, "/tmp/apache.err", m_error);
				return EXIT_SUCCESS;
			}
			res = bstmt->executeQuery();
			res->next();

			boost::scoped_ptr<std::istream> blob(res->getBlob("meta"));

			char buff[16384];
			string szxml;
			while (!blob->eof())
			{
				blob->read(buff, sizeof (buff));
				szxml.append(buff, blob->gcount());
			}
			// NOTE: clean the string if not css file
			if (m_filename.find ( ".css", 0 ) == string::npos)
			{
				cleanFile(szxml);
			}
			delete res;
			bstmt->close();
			bstmt.reset();
			///////////////////////////////////////////
			//get percentage word count for epub here//
			///////////////////////////////////////////
			m_pubpercent = (m_allwords * m_percent);
			bool isOver;
			if(m_isSingle)
			{
				isOver = false;
			}
			else
			{
				isOver = true;
			}
			long idcounter = 2; // 2 for next page then increment
			////////////////////////////////////////////////
			//Get the next pages from DB by chapter number//
			////////////////////////////////////////////////
			while ((m_pagewords <= m_pubpercent) && (isOver))
			{
				if (static_cast<long>(idcounter) > static_cast<long>(m_maxpage))
				{
					isOver = false;
					continue;
				}
				boost::scoped_ptr<sql::PreparedStatement> exstmt(conn->prepareStatement("SELECT e2.filename, e2.meta, i2.wordcount from epub2 e2, item2 i2 WHERE e2.edition_id = ? AND e2.id = i2.id and i2.isfirstpage = ?"));
				try
				{
					exstmt->setInt64(1, m_edition_id);
					exstmt->setUInt(2, idcounter);
				}
				catch (sql::SQLException x)
				{
					m_strstrm << x.what() << " in cblob Statement";
					m_error = m_strstrm.str();
					m_strstrm.str("");
					cout << HTTPContentHeader("text/html");
					LogError(cgi, "/tmp/apache.err", m_error);
					return EXIT_SUCCESS;
				}
				try
				{
					res = exstmt->executeQuery();
					res->next();
				}
				catch (sql::SQLException x)
				{
					m_strstrm << x.what() << " in cblob Statement";
					m_error = m_strstrm.str();
					m_strstrm.str("");
				}
				bool canGo;
				try
				{
					std::string m_pieceName = res->getString("filename").c_str();
					canGo = checkFileName(m_pieceName);
				}
				catch (sql::SQLException x)
				{
					canGo = false;
				}

				idcounter++; // move up id counter
				if (canGo)
				{
					unsigned long m_addpagewords = res->getUInt("wordcount");
					boost::scoped_ptr<std::istream> blob(res->getBlob("meta"));
					char buff[16384];
					string szadd;
					while (!blob->eof())
					{
						blob->read(buff, sizeof (buff));
						szadd.append(buff, blob->gcount());
					}
					// check wordcount so far
					if ((m_pagewords + m_addpagewords) < m_pubpercent)
					{
						m_pagewords = m_pagewords + m_addpagewords;
						cleanFile(szadd);
						stripHead(szadd);
						stripTail(szxml);
						szxml += szadd;
					} 
					else
					{
						isOver = false;
					}
				}
				delete res;
				exstmt->close();
				exstmt.reset();
			} 
			// -------------------------------------------------------------------------------
			//  NOTE: We have the content in szxml, now lets display as the format dictates. --- 
			// ------------------------------------------------------------------------------- 			
 			if(m_format.compare("text") == 0)
 			{
// 				boost::regex re("</?\w+\s+[^>]*>");
 				boost::regex re("<[^>]*>");
 				std::string repl = "";
 				std::string newout = boost::regex_replace(szxml, re, repl, boost::match_default | boost::format_all);
				cout << HTTPContentHeader("text/plain");
				cout << cleancodes(newout);
				flush(cout);
				delete conn;
				delete res;
				return EXIT_SUCCESS;
 			}
			basic_string <char>::size_type fpos = 0;
			basic_string <char>::size_type epos = 0;
			basic_string <char>::size_type spos = 0;
 			if(m_href.compare("On") == 0)
 			{
				// -------------------------------------------------------------------------------
				//  NOTE: now process the a href elements --- m_href switch
				// -------------------------------------------------------------------------------
				while ((fpos = szxml.find ( "href", fpos ) ) != string::npos) 
				{
					unsigned int m_id = 0;
					fpos = szxml.find ( "\"", fpos );
					epos = szxml.find ( "\"", fpos + 1 );
					string ftmp = szxml.substr(fpos + 1, epos - fpos - 1);
					string etmp = ftmp;
					if(etmp.find("..") != string::npos)
					{
						boost::replace_all(etmp, "..", "");
					}
					if(etmp.find("#") != string::npos)
					{
						basic_string <char>::size_type erpos = etmp.find("#");
						etmp.erase(erpos, etmp.length() - erpos);
					}
					boost::scoped_ptr<sql::PreparedStatement> fstmt(conn->prepareStatement("SELECT id from epub2 WHERE edition_id = ? and filename like CONCAT('%', ?)"));
					try
					{
						fstmt->setInt64(1, m_edition_id);
						fstmt->setString(2, etmp.c_str());
					}
					catch (sql::SQLException x)
					{
						m_strstrm << x.what() << " in id find Statement";
						m_error = m_strstrm.str();
						m_strstrm.str("");
						cout << HTTPContentHeader("text/html");
						LogError(cgi, "/tmp/apache.err", m_error);
						return EXIT_SUCCESS;
					}
					res = fstmt->executeQuery();
					while (res->next()) {
						m_id = res->getInt64("id");
						m_strstrm << getServer << m_id;
						string twoReplace = m_strstrm.str();
						m_strstrm.str("");
						boost::replace_all(szxml, ftmp, twoReplace);
					}
					delete res;
					fstmt->close();
					fstmt.reset();
				} // while ((fpos = szxml.find ( "href", fpos ) ) != string::npos)
			} // m_href is on
            //////////////////////////////////////////////////////////////
            //if href is off turn all href to point to harpercollins.com//
            //////////////////////////////////////////////////////////////
			else
			{
				while ((fpos = szxml.find ( "href", fpos ) ) != string::npos)
				{
					unsigned int m_id = 0;
					fpos = szxml.find ( "\"", fpos );
					epos = szxml.find ( "\"", fpos + 1 );
					string ftmp = szxml.substr(fpos + 1, epos - fpos - 1);
					string etmp = ftmp;
					if (etmp.find(".css") != string::npos)
					{
						if(etmp.find("..") != string::npos)
						{
							boost::replace_all(etmp, "..", "");
						}
						if(etmp.find("#") != string::npos)
						{
							basic_string <char>::size_type erpos = etmp.find("#");
							etmp.erase(erpos, etmp.length() - erpos);
						}
						boost::scoped_ptr<sql::PreparedStatement> fstmt(conn->prepareStatement("SELECT id from epub2 WHERE edition_id = ? and filename like CONCAT('%', ?)"));
						try
						{
							fstmt->setInt64(1, m_edition_id);
							fstmt->setString(2, etmp.c_str());
						}
						catch (sql::SQLException x)
						{
							m_strstrm << x.what() << " in id find Statement";
							m_error = m_strstrm.str();
							m_strstrm.str("");
							cout << HTTPContentHeader("text/html");
							LogError(cgi, "/tmp/apache.err", m_error);
							return EXIT_SUCCESS;
						}
						res = fstmt->executeQuery();
						while (res->next()) {
							m_id = res->getInt64("id");
							m_strstrm << getServer << m_id;
							string twoReplace = m_strstrm.str();
							m_strstrm.str("");
							boost::replace_all(szxml, ftmp, twoReplace);
						}
						delete res;
						fstmt->close();
						fstmt.reset();
					} // if (etmp.find(".css")
					else
					{
						szxml.replace(fpos + 1, epos - fpos - 1, "#");
					}
					fpos++;
				} // while ((fpos = szxml.find ( "href", fpos )
			}
			// -------------------------------------------------------------------------------
			//  NOTE: now Lets look for img src to re-point to cgi --- 
			// -------------------------------------------------------------------------------			
			spos = 0;
			fpos = 0;
			epos = 0;
			while ((fpos = szxml.find ( "<img", fpos ) ) != string::npos) 
			{
				unsigned int m_id = 0;
				fpos = szxml.find ( "src=", fpos );
				epos = szxml.find ( "\"", fpos );
				epos = szxml.find ( "\"", epos + 1 );
				string ftmp;
				try
				{
					ftmp = szxml.substr(fpos, epos - fpos);
				}
				catch (std::out_of_range& exception)
				{
					m_strstrm << exception.what() << " img results<br>";
					m_strstrm << "fpos:" << fpos << " epos:" << epos;
					m_strstrm << "ftmp:" << ftmp << "spos" << spos;
					m_error = m_strstrm.str();
					m_strstrm.str("");
					cout << HTTPContentHeader("text/html");
					LogError(cgi, "/tmp/apache.err", m_error);
					return EXIT_SUCCESS;
				}
				spos = ftmp.find ( "\"", 0 );
				string etmp = ftmp;
				etmp.erase(0, spos + 1);
				string orgImage = etmp;
				if(etmp.find("..") != string::npos)
				{
					boost::replace_all(etmp, "..", "");
				}
				////////////////////////////////////////////////////////
				//To refine search add / only if not already present !//
				////////////////////////////////////////////////////////
				if (etmp.compare(0, 1, "/") != 0)
				{
					etmp.insert(0, "/");
				}
				boost::scoped_ptr<sql::PreparedStatement> imgstmt(conn->prepareStatement("SELECT id from epub2 WHERE edition_id = ? and filename like CONCAT('%', ?)"));
				try
				{
					imgstmt->setInt64(1, m_edition_id);
					imgstmt->setString(2, etmp.c_str());
				}
				catch (sql::SQLException x)
				{
					m_strstrm << x.what() << " in id find Statement";
					m_error = m_strstrm.str();
					m_strstrm.str("");
					LogError(cgi, "/tmp/apache.err", m_error);
					return EXIT_SUCCESS;
				}
				res = imgstmt->executeQuery();
				while (res->next()) {
					m_id = res->getInt64("id");
					m_strstrm << getServer << m_id;
					string twoReplace = m_strstrm.str();
					m_strstrm.str("");
					size_t rstart, rend; 
					rstart = fpos + spos + 1;
					rend = epos - rstart;
					szxml.replace(rstart, rend, twoReplace);
//					boost::replace_all(szxml, orgImage, twoReplace);
				}
				delete res;
				imgstmt->close();
				imgstmt.reset();
				fpos = fpos + 4;
			} // while ((fpos = szxml.find ( "<img", fpos ) ) != string::npos) 
			// -------------------------------------------------------------------------------
			//  NOTE: Output the mime type to return. --- 
			// -------------------------------------------------------------------------------		
			cout << HTTPContentHeader(m_mimetype);
			cout << dec << szxml;
			flush(cout);
			return EXIT_SUCCESS;
		} 
		else // it's an image not text
		{
			m_strstrm << "Content-Type: " << m_mimetype << "\n\n";
			string mtype = m_strstrm.str();
			cout << mtype;
			flush(cout);
	
			boost::scoped_ptr<sql::PreparedStatement> bstmt(conn->prepareStatement("SELECT multi from epub2 WHERE id = ?"));
			try
			{
				if (m_isIsbn)
				{
					bstmt->setUInt(1, m_id);
				}
				else
				{
					bstmt->setUInt(1, atol(m_image.c_str()));
				}
			}
			catch (sql::SQLException x)
			{
				m_strstrm << x.what() << " blob Statement";
				m_error = m_strstrm.str();
				m_strstrm.str("");
			}
			try
			{
				res = bstmt->executeQuery();
				res->next();
			}
			catch (sql::SQLException x)
			{
				m_strstrm << x.what() << " blob Statement";
				m_error = m_strstrm.str();
				m_strstrm.str("");
			}
			std::auto_ptr<std::istream> blob_output_stream(res->getBlob("multi"));
			boost::scoped_array<char> blob_out(new char[m_multiLen]);
			blob_output_stream->read(blob_out.get(), m_multiLen);
			char *buff = new char[8192];
			blob_output_stream->seekg(std::ios::beg);
			string filename;
			std::string tempDir;
	#if defined(WIN32)
			DWORD dwRetVal, dwBufSize;
			dwBufSize = BUFSIZE;
			char buffer[BUFSIZE];
			dwRetVal = GetTempPath(dwBufSize, buffer);
			tempDir = buffer;
	#else
			tempDir = "/tmp/";
	#endif
			filename = tempDir += PATH_SEPARATOR;
			filename += m_filename;
			ofstream output;
			long totalbytes = 0;
			while (!blob_output_stream->eof())
			{
				blob_output_stream->read(buff, 8192);
				std::streamsize bytesRead = blob_output_stream->gcount();
				cout.write(buff, bytesRead);
				flush(cout);
				totalbytes = totalbytes + bytesRead;
			}
			delete res;
			bstmt->close();
			bstmt.reset();
		}
		delete conn;
		// -------------------------------------------------------------------------------
		//  This is one delete to many, segfault if used. --- 
		// -------------------------------------------------------------------------------		
//		delete res;
		return EXIT_SUCCESS;
	}
	catch (const std::exception& e)
	{
		html::reset(); 	head::reset(); 		body::reset();
		title::reset(); 	h1::reset(); 		h4::reset();
		comment::reset(); 	td::reset(); 		tr::reset(); 
		table::reset();	cgicc::div::reset(); 	p::reset(); 
		a::reset();		h2::reset(); 		colgroup::reset();

//		cout << HTTPContentHeader("text/html");
		// Output the HTTP headers for an HTML document, and the HTML 4.0 DTD info
		cout << HTTPHTMLHeader() << HTMLDoctype(HTMLDoctype::eStrict) << endl;
		cout << html().set("lang","en").set("dir","ltr") << endl;

		// Set up the page's header and title.
		// I will put in lfs to ease reading of the produced HTML. 
		cout << head() << endl;

		// Output the style sheet portion of the header
		cout << style() << comment() << endl;
		cout << "body { color: black; background-color: white; }" << endl;
		cout << "hr.half { width: 60%; align: center; }" << endl;
		cout << "span.red, STRONG.red { color: red; }" << endl;
		cout << "div.notice { border: solid thin; padding: 1em; margin: 1em 0; "
			<< "background: #ddd; }" << endl;

		cout << comment() << style() << endl;

		cout << title("epubFetch had a problem") << endl;
		cout << head() << endl;

		cout << body() << endl;

		cout << h1() << "epubFetch cgi" << span("cc", cgicc::set("class","red"))
			<< " caught a main exception" << h1() << endl; 

		cout << cgicc::div().set("align","center").set("class","notice") << endl;

		cout << h2(e.what()) << endl;

		// End of document
		cout << cgicc::div() << endl;
		cout << hr().set("class","half") << endl;
		cout << body() << html() << endl;

		return EXIT_SUCCESS;
	}
	// Fat lady sings
	return 0;
}

// -------------------------------------------------------------------------------
//  LogError --- 
// -------------------------------------------------------------------------------
void LogError(const Cgicc& cgi, string lFile, string pMsg)
{
		html::reset(); 	head::reset(); 		body::reset();
		title::reset(); 	h1::reset(); 		h4::reset();
		comment::reset(); 	td::reset(); 		tr::reset(); 
		table::reset();	cgicc::div::reset(); 	p::reset(); 
		a::reset();		h2::reset(); 		colgroup::reset();

		lFile.erase(0);
//		cout << HTTPContentHeader("text/html");

		// Output the HTTP headers for an HTML document, and the HTML 4.0 DTD info
		cout << HTTPHTMLHeader() << HTMLDoctype(HTMLDoctype::eStrict) << endl;
		cout << html().set("lang","en").set("dir","ltr") << endl;

		// Set up the page's header and title.
		// I will put in lfs to ease reading of the produced HTML. 
		cout << head() << endl;

		// Output the style sheet portion of the header
		cout << style() << comment() << endl;
		cout << "body { color: black; background-color: white; }" << endl;
		cout << "hr.half { width: 60%; align: center; }" << endl;
		cout << "span.red, STRONG.red { color: red; }" << endl;
		cout << "div.notice { border: solid thin; padding: 1em; margin: 1em 0; "
			<< "background: #ddd; }" << endl;

		cout << comment() << style() << endl;

		cout << title("epubFetch problem") << endl;
		cout << head() << endl;

		cout << body() << endl;

		cout << h1() << "epub" << span("Fetch", cgicc::set("class","red"))
			<< " unable to display this book" << h1() << endl; 

		cout << cgicc::div().set("align","center").set("class","notice") << endl;

		cout << h2(pMsg) << endl;

		// End of document
		cout << cgicc::div() << endl;
		cout << hr().set("class","half") << endl;
		cout << body() << html() << endl;

//		return EXIT_SUCCESS;
		return;
// 	string m_outMsg;
// 	std::ostringstream m_strstrm;
// 	/****** format strings ******/
// 	const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
// 	boost::posix_time::time_facet*const f = new boost::posix_time::time_facet("%a %b %d, %H-%M-%S");
// 	m_strstrm.imbue(std::locale(m_strstrm.getloc(),f));
// 	m_strstrm << now;
// 
// 
// 	m_strstrm << " : " << pMsg << endl;
// 	m_outMsg = m_strstrm.str();
// 	m_strstrm.str("");
// 	fstream myfile;
// 	//open for appending and append
// 	myfile.open(lFile.c_str(), ios::out | ios::app);
// 	if (!myfile.is_open())
// 	{
// 		std::cout << "The Log file did not open." << endl << pMsg << endl;
// 	}
// 	myfile << m_outMsg.c_str();
// 	myfile.close();
#ifdef _VERBOSE
	std::cout << pMsg;
#endif
}
// -------------------------------------------------------------------------------
//  string cleancodes(string& szcontent) --- remove html entity codes from text
// -------------------------------------------------------------------------------
string cleancodes(string& szcontent)
{
	if(szcontent.find("&#8230;") != string::npos)
	{
		boost::replace_all(szcontent, "&#8230;", "'");
	}
	
	if(szcontent.find("&#8220;") != string::npos)
	{
		boost::replace_all(szcontent, "&#8220;", "\"");
	}
	
	if(szcontent.find("&#8221;") != string::npos)
	{
		boost::replace_all(szcontent, "&#8221;", "\"");
	}
	
	if(szcontent.find("&#8217;") != string::npos)
	{
		boost::replace_all(szcontent, "&#8217;", "'");
	}
	
	if(szcontent.find("&#8216;") != string::npos)
	{
		boost::replace_all(szcontent, "&#8216;", "'");
	}
	
	if(szcontent.find("&#8211;") != string::npos)
	{
		boost::replace_all(szcontent, "&#8211;", "-");
	}
	
	if(szcontent.find("&#8212;") != string::npos)
	{
		boost::replace_all(szcontent, "&#8212;", "-");
	}
	
	if(szcontent.find("&#38;") != string::npos)
	{
		boost::replace_all(szcontent, "&#38;", "&");
	}
	
	if(szcontent.find("&rsquo;") != string::npos)
	{
		boost::replace_all(szcontent, "&rsquo;", "'");
	}
	
	if(szcontent.find("&lsquo;") != string::npos)
	{
		boost::replace_all(szcontent, "&lsquo;", "'");
	}
	
	if(szcontent.find("&mdash;") != string::npos)
	{
		boost::replace_all(szcontent, "&mdash;", "-");
	}
	
	if(szcontent.find("&ldquo;") != string::npos)
	{
		boost::replace_all(szcontent, "&ldquo;", "\"");
	}
	
	if(szcontent.find("&rdquo;") != string::npos)
	{
		boost::replace_all(szcontent, "&rdquo;", "\"");
	}
	
	if(szcontent.find("&hellip;") != string::npos)
	{
		boost::replace_all(szcontent, "&hellip;", "...");
	}
	
	if(szcontent.find("&#160;") != string::npos)
	{
		boost::replace_all(szcontent, "&#160;", " ");
	}
	
	if(szcontent.find("&nbsp;") != string::npos)
	{
		boost::replace_all(szcontent, "&nbsp;", " ");
	}
	
	if(szcontent.find("&uuml;") != string::npos)
	{
		boost::replace_all(szcontent, "&uuml;", "u");
	}
	
	if(szcontent.find("&eacute;") != string::npos)
	{
		boost::replace_all(szcontent, "&eacute;", "e");
	}
	
	if(szcontent.find("&#60;") != string::npos)
	{
		boost::replace_all(szcontent, "&#60;", "<");
	}
	
	if(szcontent.find("&#62;") != string::npos)
	{
		boost::replace_all(szcontent, "&#62;", ">");
	}
	
	if(szcontent.find("&#243;") != string::npos)
	{
		boost::replace_all(szcontent, "&#243;", "o");
	}
	
	if(szcontent.find("&ecirc;") != string::npos)
	{
		boost::replace_all(szcontent, "&ecirc;", "e");
	}
	
	if(szcontent.find("&acirc;") != string::npos)
	{
		boost::replace_all(szcontent, "&acirc;", "a");
	}
	
	if(szcontent.find("&agrave;") != string::npos)
	{
		boost::replace_all(szcontent, "&agrave;", "a");
	}
	
	return szcontent;
}
// -------------------------------------------------------------------------------
//  stripHead(string& sznewpage) --- Clean file for new output
// -------------------------------------------------------------------------------
string stripHead(string& sznewpage)
{
	basic_string <char>::size_type firsttag = 0;
	firsttag = sznewpage.find("<body", 0);
	if(firsttag != 0)
	{
		basic_string <char>::size_type lastmark = 0;
		lastmark = sznewpage.find(">", firsttag);
		if(lastmark != 0)
		{
			sznewpage.erase(0, lastmark + 1);
		}
	}
	return sznewpage;
}
// -------------------------------------------------------------------------------
//  stripTail(string& sznewpage) --- Clean end of file.
// -------------------------------------------------------------------------------
string stripTail(string& sznewpage)
{
	// FIXME: look for </body> or </xml> to fix.
	basic_string <char>::size_type lasttag = 0;
	lasttag = sznewpage.find("</body>", 0); //
	if(lasttag != string::npos)
	{
		sznewpage.erase(lasttag , sznewpage.length() - lasttag);
	}
	return sznewpage;
}
// -------------------------------------------------------------------------------
//  cleanFile(string& sznewpage) --- Clean head of file.
// -------------------------------------------------------------------------------
string cleanFile(string& sznewpage)
{
	// moved to function from inline
	basic_string <char>::size_type firstchar = 0;
	firstchar = sznewpage.find("<", 0);
	if(firstchar != 0)
	{
		sznewpage.erase(0, firstchar);
	}
	// FIXME: look for </html> or </xml> to fix.
	basic_string <char>::size_type lastchar = 0;
	lastchar = sznewpage.find("</htm", 0); //
	if(lastchar != string::npos)
	{
		basic_string <char>::size_type lastmark = 0;
		lastmark = sznewpage.find(">", lastchar);
		if(lastmark != sznewpage.length() - 1)
		{
			sznewpage.erase(lastmark +1, sznewpage.length() - lastmark);
		}
	}
	return sznewpage;
}
// -------------------------------------------------------------------------------
//  checkFileName(const string& filename) --- Are we goog to display ?
// -------------------------------------------------------------------------------
bool checkFileName(const string& filename)
{
	if(filename.length() == 0)
		return false;

	if((filename.find(".htm") != string::npos) || (filename.find(".xhtm") != string::npos))
		return true;

	return false;
}
