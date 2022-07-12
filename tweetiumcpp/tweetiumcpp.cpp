#include <iostream>
#include <map>
#include <iostream>
#include <string>
#include <chrono>
#include <curl/curl.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <regex>

//#pragma warning(disable : 4996) //_CRT_SECURE_NO_WARNINGS

using namespace boost::posix_time;
using namespace std;
using boost::property_tree::ptree;

void debug_print(string text) {
    std::cout << text << std::endl;
}

template <typename T = boost::property_tree::ptree>
T element_at(ptree const& pt, std::string name, size_t n) {
    return std::next(pt.get_child(name).find(""), n)->second.get_value<T>();
}

size_t curlwrite_callback(void* contents, size_t size, size_t nmemb, std::string* s)
{
    size_t newLength = size * nmemb;
    try
    {
        s->append((char*)contents, newLength);
    }
    catch (std::bad_alloc& e)
    {
        return 0;
    }
    return newLength;
}


/*
std::string get_guest_token()
{
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    std::string json_string;
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.twitter.com/1.1/guest/activate.json");
        struct curl_slist* chunk = NULL;
        chunk = curl_slist_append(chunk, "Authorization: Bearer AAAAAAAAAAAAAAAAAAAAANRILgAAAAAAnNwIzUejRCOuH5E6I8xnZz4puTs%3D1Zv7ttfk8LF81IUq16cHjhLTvJu4FA33AGWWjCpTnA");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_string);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "name=ClipperFrog&email=ClipperFrog@gmail.com");
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
            return "";
        }
        curl_easy_cleanup(curl);

        auto json = padded_string(json_string);
        dom::parser parser;
        auto doc = parser.parse(json);
        simdjson::dom::element token = doc["guest_token"];
        std::string_view token2 = token.get_string();
        std::string s = { token2.begin(), token2.end() };
        return s;
    }

    return "";
}*/


std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

std::string create_url_params(std::string base_url, std::vector<std::pair<std::string, std::string>> query_params) {
    int el_count = 0;

    for (auto& element : query_params) {
        if (el_count >= 1) {
            base_url.append("&");
        }
        base_url.append(element.first);
        base_url.append("=");

        element.second = ReplaceAll(element.second, std::string(" "), std::string("%20"));

        base_url.append(element.second);
        el_count++;
    }
    return base_url;
}

std::string extract_cursor(boost::property_tree::ptree timeline) {
    int instruction_count = timeline.get_child("instructions").size();

    if (instruction_count >= 1) {
        if (instruction_count == 1) {
            ptree entries = timeline.get_child("instructions").front().second.get_child("addEntries").get_child("entries");
            ptree cursor = entries.back().second.get_child("content").get_child("operation").get_child("cursor");
            string cursor_value = cursor.get<string>("value");
            return cursor_value;
            
        }
        else {

            ptree entry = timeline.get_child("instructions").back().second.get_child("replaceEntry").get_child("entry");
            ptree cursor = entry.get_child("content").get_child("operation").get_child("cursor");
            string cursor_value = cursor.get<string>("value");
            return cursor_value;
            
        }
    }
    return "NULL";
}


class Tweet {
public:
    int64_t id;
    int64_t retweet_count;
    int64_t favorite_count;
    int64_t user_id;
    string text;

    Tweet(ptree tweet_data) {
        id = tweet_data.get<int64_t>("id");
        retweet_count = tweet_data.get<int64_t>("retweet_count");
        favorite_count = tweet_data.get<int64_t>("favorite_count");
        user_id = tweet_data.get<int64_t>("user_id");
        text = tweet_data.get<string>("text");
    }
};

class TweetResponse {
public:
    vector<Tweet> tweets;
    int get_tweet_count();
    string cursor;
    bool is_last_request;

    Tweet get_tweet_at(int);
    string get_cursor();
    void print_tweets();

    TweetResponse(ptree d) {
        ptree tweets_raw_data = d.get_child("globalObjects.tweets");
        ptree timeline = d.get_child("timeline");
        cursor = extract_cursor(timeline);
        bool hasTweets = false;
        for (auto pair : tweets_raw_data)
        {
            Tweet tweet(pair.second);
            tweets.push_back(tweet);
            hasTweets = true;
        }
        if (!hasTweets) {
            is_last_request = true;
        }
    }
};

int TweetResponse::get_tweet_count() {
    return this->tweets.size();
}

string TweetResponse::get_cursor() {
    return this->cursor;
}

Tweet TweetResponse::get_tweet_at(int pos) {
    return this->tweets.at(pos);
}

void TweetResponse::print_tweets() {
    for (int i = 0; i < this->get_tweet_count(); i++) {
        Tweet t = this->get_tweet_at(i);

        std::string testString(t.text);
        std::regex e("([^\n]|^)\n([^\n]|$)");
        std::cout << "\033[3;104;30m" << " - " << "\033[0m\t\t" <<  std::regex_replace(testString, e, "$1 $2") << std::endl;
    }
}

TweetResponse make_tweet_request(std::string url) {
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    std::string json_string;
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        struct curl_slist* chunk = NULL;
        chunk = curl_slist_append(chunk, "Authorization: Bearer AAAAAAAAAAAAAAAAAAAAANRILgAAAAAAnNwIzUejRCOuH5E6I8xnZz4puTs%3D1Zv7ttfk8LF81IUq16cHjhLTvJu4FA33AGWWjCpTnA");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_string);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);

        std::stringstream ss;

        ss << json_string;

        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);
        TweetResponse tweetResponse(pt);

        return tweetResponse;


    }
}

TweetResponse prep_tweet_request(std::string search_query, string cursor = "") {

    std::vector<std::pair<std::string, std::string>> query_params;
    query_params.push_back(std::make_pair("q", search_query.c_str()));
    query_params.push_back(std::make_pair("src", "typed_query"));
    query_params.push_back(std::make_pair("f", "live"));
    
    if (cursor != "") {
        query_params.push_back(std::make_pair("cursor", cursor));
    }

    std::string base_url = "https://api.twitter.com/2/search/adaptive.json?";
    std::string url_full = create_url_params(base_url, query_params);
    return make_tweet_request(url_full);
    
}


void get_tweets(string search_query, string since = "", string until = "", double longitude = 0, double latitude = 0){
    try {

        string cursor = "";
        int total_tweet_count = 0;
        bool done = false;

        while (!done) {
            std::stringstream ss;

            ss << search_query;

            if (since != "" && until != "") {
                ss << " until:" << until << " since:" << since << " ";
            }

            if (longitude != 0 && latitude != 0) {
                ss << " geocode:" << longitude << "," << latitude << ",55.5km ";
            }

            string tweetParams = ss.str();
            
            TweetResponse tweetResponse = prep_tweet_request(tweetParams, cursor);

            tweetResponse.print_tweets();

            total_tweet_count += tweetResponse.get_tweet_count();
            cursor = tweetResponse.get_cursor();
            done = tweetResponse.is_last_request;
            
            std::cout << "\033[3;43;30m" << "Total tweets loaded: "  << total_tweet_count << "\033[0m\t\t" << endl;
        }

    }
    catch (std::exception const& ex)
    {
        cout << ("Can't init settings. %s", ex.what());
    } 
}

int date_to_epoch(std::string date) {
    std::stringstream ss;
    ss << date << " 00:00:00";
    std::string ts(ss.str());
    ptime t(time_from_string(ts));
    ptime start(boost::gregorian::date::date(1970, 1, 1));
    time_duration dur = t - start;
    time_t epoch = dur.total_seconds();
    return epoch;
}

string epoch_to_date(int epochTime) {
    std::stringstream str;
    boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%Y-%m-%d");
    str.imbue(std::locale(str.getloc(), facet));
    str << boost::posix_time::from_time_t(epochTime);
    return str.str();
}

string get_current_date() {
    std::stringstream str;
    boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%Y-%m-%d");
    str.imbue(std::locale(str.getloc(), facet));
    str << boost::posix_time::second_clock::universal_time(); //your time point goes here
    return str.str();
}

int main() {
    /*
    std::cout << date_to_epoch("2022-07-12") << endl;
    std::cout << epoch_to_date(1657584000) << endl;
    std::cout << "\n\n" << get_current_date() << endl;*/

}





/*-
int main() {

    std::string scan_from_date = "2010-01-01";
    std::string scan_date_to = "2021-12-18";

    int current_scan_epoch = date_to_epoch(scan_from_date);
    int end_scan_epoch = date_to_epoch(scan_date_to);

    bool scan_finish = false;

    while (!scan_finish) {

        if (current_scan_epoch <= end_scan_epoch) {
            string date = epoch_to_date(current_scan_epoch);
            int i = current_scan_epoch;
            current_scan_epoch += 86400;
            int j = current_scan_epoch;

            string date_from = date;
            string date_to = epoch_to_date(current_scan_epoch);

            std::cout << "Scanning date: " << date_from <<" to " << date_to << endl;
            
        }
        else {
            scan_finish = true;
        }
        
    }

    

}
*/


















/* 
TODO:

- Update create_url_params function to use char list instead of string https://www.geeksforgeeks.org/methods-to-concatenate-string-in-c-c-with-examples/
- LIBCURL Optimizations as saved on bookmarks
- Checkout cursor placement, do we get more results from using scroll:CURSOR

            //ALSO GET TWEET COUNT AS METRIC
            // GET TWEEET METRICS 

            //LOAD USERS DATA



            // CHANGE scan_date_to to use current date
*/


/*
int main(int argc, char** argv) {

    printf("\n");
    printf("\x1B[31mTexting1\033[0m\t\t");
    printf("\x1B[32mTexting2\033[0m\t\t");
    printf("\x1B[33mTexting3\033[0m\t\t");
    printf("\x1B[34mTexting4\033[0m\t\t");
    printf("\x1B[35mTexting5\033[0m\n");

    printf("\x1B[36mTexting6\033[0m\t\t");
    printf("\x1B[36mTexting7\033[0m\t\t");
    printf("\x1B[36mTexting8\033[0m\t\t");
    printf("\x1B[37mTexting9\033[0m\t\t");
    printf("\x1B[93mTexting10\033[0m\n");

    printf("\033[3;42;30mTexting11\033[0m\t\t");
    printf("\033[3;43;30mTexting12\033[0m\t\t");
    printf("\033[3;44;30mTexting13\033[0m\t\t");
    printf("\033[3;104;30mTexting14\033[0m\t\t");
    printf("\033[3;100;30mTexting15\033[0m\n");

    printf("\033[3;47;35mTexting16\033[0m\t\t");
    printf("\033[2;47;35mTexting17\033[0m\t\t");
    printf("\033[1;47;35mTexting18\033[0m\t\t");
    printf("\t\t");
    printf("\n");

    return 0;
}
*/