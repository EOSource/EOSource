#ifndef I18N_HPP_INCLUDED
#define I18N_HPP_INCLUDED

#include <memory>
#include <utility>
#include <vector>

#include "fwd/config.hpp"
#include "util/variant.hpp"

class I18N
{
	protected:
		std::unique_ptr<Config> lang_config;

	public:
		I18N();
		I18N(const std::string& lang_file);

		void SetLangFile(const std::string& lang_file);

		std::string FormatV(const std::string& id, std::vector<util::variant> &&v) const;

		template <class... Args> std::string Format(const std::string& id, Args&&... args) const
		{
			std::vector<util::variant> v{args...};
			return FormatV(id, std::move(v));
		}

		~I18N();
};

#endif
