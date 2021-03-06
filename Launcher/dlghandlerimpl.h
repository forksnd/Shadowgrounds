#ifndef INC_DLGHANDLERIMPL_H
#define INC_DLGHANDLERIMPL_H

#pragma warning(disable:4786)

#include "idlghandler.h"

#include <string>
#include <vector>
#include <map>

namespace frozenbyte {
namespace launcher {

class DlgHandlerImpl : public IDlgHandler
{
public:
	DlgHandlerImpl() : localizationComboBox() { }
	virtual ~DlgHandlerImpl() { }

	virtual BOOL handleMessages(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	
	virtual std::string getComboBoxSelection( HWND hwnd );
	virtual void		addComboItems( HWND hwnd, const std::vector< std::string >& items, const std::string& select = "" );
	virtual void		setCheckBox( HWND hwnd, int buttonId, const std::string& value );
	virtual std::string getCheckBoxValue( HWND hwnd, int buttonId );

	virtual void		setDescriptionText( HWND hwnd );

protected:
#ifdef FB_RU_HAX
	std::map< std::wstring, std::string > localizationComboBox;
#else
	std::map< std::string, std::string > localizationComboBox;
#endif

};

} // end of namespace launcher
} // end of namespace frozenbyte



#endif