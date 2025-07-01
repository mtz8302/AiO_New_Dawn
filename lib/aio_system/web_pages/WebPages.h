// WebPages.h
// Central include for all web pages with language selection

#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>
#include "CommonStyles.h"

// Language selection enum
enum class WebLanguage : uint8_t {
    ENGLISH = 0,
    GERMAN = 1,
    FRENCH = 2,
    SPANISH = 3
};

// Include all language versions
#include "en_HomePage.h"
#include "en_EventLoggerPage.h"
#include "en_NetworkPage.h"
#include "en_OTAPage.h"
#include "de_HomePage.h"
#include "de_EventLoggerPage.h"
// Add more languages as needed:
// #include "fr_HomePage.h"
// #include "fr_EventLoggerPage.h"

// Helper class to get correct page based on language setting
class WebPageSelector {
public:
    static const char* getHomePage(WebLanguage lang) {
        switch(lang) {
            case WebLanguage::GERMAN:
                return DE_HOME_PAGE;
            default:
                return EN_HOME_PAGE;
        }
    }
    
    static const char* getEventLoggerPage(WebLanguage lang) {
        switch(lang) {
            case WebLanguage::GERMAN:
                return DE_EVENTLOGGER_PAGE;
            default:
                return EN_EVENTLOGGER_PAGE;
        }
    }
    
    static const char* getNetworkPage(WebLanguage lang) {
        // Only English version for now
        return EN_NETWORK_PAGE;
    }
    
    static const char* getOTAPage(WebLanguage lang) {
        // Only English version for now
        return EN_OTA_PAGE;
    }
    
    // Add getters for each page type as you implement them
};

#endif // WEB_PAGES_H