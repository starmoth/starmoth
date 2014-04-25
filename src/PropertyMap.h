// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef PROPERTYMAP_H
#define PROPERTYMAP_H

#include "Serializer.h"
#include "libs.h"

class PropertyMap {
public:
	PropertyMap();

	template <class Value> void Set(const std::string &k, const Value &v) {
		// XXX ScopedTable(m_table).Set(k, v);
		SendSignal(k);
	}

	template <class Value> void Get(const std::string &k, Value &v) {
		// XXX v = ScopedTable(m_table).Get<Value>(k, v);
	}

	sigc::connection Connect(const std::string &k, const sigc::slot<void,PropertyMap &,const std::string &> &fn) {
		return m_signals[k].connect(fn);
	}

	void Save(Serializer::Writer &wr) {
		// XXX m_table.Save(wr);
	}
	void Load(Serializer::Reader &rd) {
		// XXX m_table.Load(rd);
	}

private:
	void SendSignal(const std::string &k);
	std::map< std::string,sigc::signal<void,PropertyMap &,const std::string &> > m_signals;
};

#endif
