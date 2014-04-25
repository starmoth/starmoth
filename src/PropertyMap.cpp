// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "PropertyMap.h"

PropertyMap::PropertyMap()
{
}

void PropertyMap::SendSignal(const std::string &k)
{
	std::map< std::string,sigc::signal<void,PropertyMap &,const std::string &> >::iterator i = m_signals.find(k);
	if (i == m_signals.end())
		return;

	(*i).second.emit(*this, k);
}
