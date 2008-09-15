/*!
	@file
	@author		Georgiy Evmenov
	@date		09/2008
	@module
*/

#include "PanelProperties.h"
#include "WidgetContainer.h"
#include "WidgetTypes.h"
#include "EditorState.h" //FIXME_HOOK

inline const Ogre::UTFString localise(const Ogre::UTFString & _str) {
	return MyGUI::LanguageManager::getInstance().getTag(_str);
}

PanelProperties::PanelProperties() :
	BaseLayout("PanelProperties.layout"),
	PanelBase()
{
}

void PanelProperties::initialiseCell(PanelCell * _cell)
{
	PanelBase::initialiseCell(_cell);

	loadLayout(_cell->getClient());
	mMainWidget->setPosition(0, 0, _cell->getClient()->getWidth(), mMainWidget->getHeight());
	_cell->setCaption(localise("Widget_type_propertes"));
}

void PanelProperties::shutdownCell()
{
	PanelBase::shutdownCell();

	BaseLayout::shutdown();
}

void PanelProperties::update(MyGUI::WidgetPtr _current_widget, PropertiesGroup _group)
{
	int y = 0;

	WidgetType * widgetType = WidgetTypes::getInstance().find(_current_widget->getTypeName());
	WidgetContainer * widgetContainer = EditorWidgets::getInstance().find(_current_widget);

	if (_group == TYPE_PROPERTIES)
	{
		MyGUI::LanguageManager::getInstance().addTag("widget_type", _current_widget->getTypeName());
		if (widgetType->name == "Widget")
		{
			if (_current_widget->getTypeName() != "Widget")
			{
				mPanelCell->setCaption(MyGUI::LanguageManager::getInstance().replaceTags(localise("Properties_not_available")));
				y += PropertyItemHeight;
			}
		}
		else
		{
			mPanelCell->setCaption(MyGUI::LanguageManager::getInstance().replaceTags(localise("Widget_type_propertes")));

			for (StringPairs::iterator iter = widgetType->parameter.begin(); iter != widgetType->parameter.end(); ++iter)
			{
				std::string value = "";
				for (StringPairs::iterator iterProperty = widgetContainer->mProperty.begin(); iterProperty != widgetContainer->mProperty.end(); ++iterProperty)
					if (iterProperty->first == iter->first){ value = iterProperty->second; break;}
				mEditor.createPropertiesWidgetsPair(mWidgetClient, iter->first, value, iter->second, y);
				y += PropertyItemHeight;
			}
			/*if (widgetType->parameter.empty()) mPanelCell->hide();
			else mPanelCell->show;*/
		}
	}
	else if (_group == WIDGET_PROPERTIES || _group == EVENTS)
	{
		if (_group == WIDGET_PROPERTIES) mPanelCell->setCaption(localise("Other_properties"));
		else mPanelCell->setCaption(localise("Events"));

		if (_current_widget->getTypeName() != "Sheet")
		{
			//mPanelCell->show();
			//base properties (from Widget)
			WidgetType * baseType = WidgetTypes::getInstance().find("Widget");
			for (StringPairs::iterator iter = baseType->parameter.begin(); iter != baseType->parameter.end(); ++iter)
			{
				std::string value = "";
				for (StringPairs::iterator iterProperty = widgetContainer->mProperty.begin(); iterProperty != widgetContainer->mProperty.end(); ++iterProperty)
					if (iterProperty->first == iter->first){ value = iterProperty->second; break;}
				if ((0 == strncmp("Widget_event", iter->first.c_str(), 12)) ^ (_group == WIDGET_PROPERTIES))
				{
					mEditor.createPropertiesWidgetsPair(mWidgetClient, iter->first, value, iter->second, y);
					y += PropertyItemHeight;
				}
			}
		}
		else
		{
			//mPanelCell->hide();
		}
	}

	mPanelCell->setClientHeight(y, true);
}
