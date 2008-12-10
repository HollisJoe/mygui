/*!
	@file
	@author		Albert Semenov
	@date		12/2007
	@module
*/
#include "MyGUI_ComboBox.h"
#include "MyGUI_ControllerManager.h"
#include "MyGUI_InputManager.h"
#include "MyGUI_WidgetManager.h"
#include "MyGUI_Gui.h"
#include "MyGUI_ControllerFadeAlpha.h"
#include "MyGUI_List.h"
#include "MyGUI_Button.h"
#include "MyGUI_WidgetSkinInfo.h"
#include "MyGUI_LayerManager.h"

namespace MyGUI
{

	//MYGUI_RTTI_CHILD_IMPLEMENT2( ComboBox, Edit );

	const float COMBO_ALPHA_MAX  = ALPHA_MAX;
	const float COMBO_ALPHA_MIN  = ALPHA_MIN;
	const float COMBO_ALPHA_COEF = 4.0f;

	ComboBox::ComboBox(WidgetType _behaviour, const IntCoord& _coord, Align _align, const WidgetSkinInfoPtr _info, WidgetPtr _parent, ICroppedRectangle * _croppedParent, IWidgetCreator * _creator, const std::string & _name) :
		Edit(_behaviour, _coord, _align, _info, _parent, _croppedParent, _creator, _name),
		mButton(null),
		mList(null),
		mListShow(false),
		mMaxHeight(0),
		mItemIndex(ITEM_NONE),
		mModeDrop(false),
		mDropMouse(false),
		mShowSmooth(false)
	{
		initialiseWidgetSkin(_info);
	}

	ComboBox::~ComboBox()
	{
		shutdownWidgetSkin();
	}

	void ComboBox::baseChangeWidgetSkin(WidgetSkinInfoPtr _info)
	{
		shutdownWidgetSkin();
		Edit::baseChangeWidgetSkin(_info);
		initialiseWidgetSkin(_info);
	}

	void ComboBox::initialiseWidgetSkin(WidgetSkinInfoPtr _info)
	{
		// парсим свойства
		const MapString & properties = _info->getProperties();
		MapString::const_iterator iter = properties.find("HeightList");
		if (iter != properties.end()) mMaxHeight = utility::parseSizeT(iter->second);

		iter = properties.find("ListSmoothShow");
		if (iter != properties.end()) setSmoothShow(utility::parseBool(iter->second));

		std::string listSkin, listLayer;
		iter = properties.find("ListSkin");
		if (iter != properties.end()) listSkin = iter->second;
		iter = properties.find("ListLayer");
		if (iter != properties.end()) listLayer = iter->second;

		// ручками создаем список
		//FIXME
		mList = createWidget<List>(WidgetType::Popup, listSkin, IntCoord(), Align::Default, listLayer);
		mWidgetChild.pop_back();

		mList->hide();
		mList->eventKeyLostFocus = newDelegate(this, &ComboBox::notifyListLostFocus);
		mList->eventListSelectAccept = newDelegate(this, &ComboBox::notifyListSelectAccept);
		mList->eventListMouseItemActivate = newDelegate(this, &ComboBox::notifyListMouseItemActivate);
		mList->eventListChangePosition = newDelegate(this, &ComboBox::notifyListChangePosition);

		// парсим кнопку
		for (VectorWidgetPtr::iterator iter=mWidgetChildSkin.begin(); iter!=mWidgetChildSkin.end(); ++iter) {
			if (*(*iter)->_getInternalData<std::string>() == "Button") {
				MYGUI_DEBUG_ASSERT( ! mButton, "widget already assigned");
				mButton = (*iter)->castType<Button>();
				mButton->eventMouseButtonPressed = newDelegate(this, &ComboBox::notifyButtonPressed);
			}
		}
		MYGUI_ASSERT(null != mButton, "Child Button not found in skin (combobox must have Button)");

		// корректируем высоту списка
		if (mMaxHeight < mList->getFontHeight()) mMaxHeight = mList->getFontHeight();

		// подписываем дочерние классы на скролл
		mWidgetClient->eventMouseWheel = newDelegate(this, &ComboBox::notifyMouseWheel);
		mWidgetClient->eventMouseButtonPressed = newDelegate(this, &ComboBox::notifyMousePressed);

		// подписываемся на изменения текста
		eventEditTextChange = newDelegate(this, &ComboBox::notifyEditTextChange);
	}

	void ComboBox::shutdownWidgetSkin()
	{
		//FIXME чтобы теперь удалить, виджет должен быть в нашем списке
		mWidgetChild.push_back(mList);
		WidgetManager::getInstance().destroyWidget(mList);
		mList = null;
		mButton = null;
	}


	void ComboBox::notifyButtonPressed(WidgetPtr _sender, int _left, int _top, MouseButton _id)
	{
		if (MB_Left != _id) return;

		mDropMouse = true;

		if (mListShow) hideList();
		else showList();
	}

	void ComboBox::notifyListLostFocus(WidgetPtr _sender, WidgetPtr _new)
	{
		if (mDropMouse) {
			mDropMouse = false;
			WidgetPtr focus = InputManager::getInstance().getMouseFocusWidget();
			// кнопка сама уберет список
			if (focus == mButton) return;
			// в режиме дропа все окна учавствуют
			if ( (mModeDrop) && (focus == mWidgetClient) ) return;
		}

		hideList();
	}

	void ComboBox::notifyListSelectAccept(WidgetPtr _widget, size_t _position)
	{
		mItemIndex = _position;
		Edit::setCaption(mItemIndex != ITEM_NONE ? mList->getItemNameAt(mItemIndex) : "");

		mDropMouse = false;
		InputManager::getInstance().setKeyFocusWidget(this);

		if (mModeDrop) {
			eventComboAccept.m_eventObsolete(this);
			eventComboAccept.m_event(this, mItemIndex);
		}
	}

	void ComboBox::notifyListChangePosition(WidgetPtr _widget, size_t _position)
	{
		mItemIndex = _position;
		eventComboChangePosition(this, _position);
	}

	void ComboBox::onKeyButtonPressed(KeyCode _key, Char _char)
	{
		Edit::onKeyButtonPressed(_key, _char);

		// при нажатии вниз, показываем лист
		if (_key == KC_DOWN) {
			// выкидываем список только если мыша свободна
			if (false == InputManager::getInstance().isCaptureMouse()) {
				showList();
			}
		}
		// нажат ввод в окне редиктирования
		else if ((_key == KC_RETURN) || (_key == KC_NUMPADENTER)) {
			eventComboAccept.m_eventObsolete(this);
			eventComboAccept.m_event(this, mItemIndex);
		}

	}

	void ComboBox::notifyListMouseItemActivate(WidgetPtr _widget, size_t _position)
	{
		mItemIndex = _position;
		Edit::setCaption(mItemIndex != ITEM_NONE ? mList->getItemNameAt(mItemIndex) : "");

		InputManager::getInstance().setKeyFocusWidget(this);

		if (mModeDrop) {
			eventComboAccept.m_eventObsolete(this);
			eventComboAccept.m_event(this, mItemIndex);
		}
	}

	void ComboBox::notifyMouseWheel(WidgetPtr _sender, int _rel)
	{
		if (mList->getItemCount() == 0) return;
		if (InputManager::getInstance().getKeyFocusWidget() != this) return;
		if (InputManager::getInstance().isCaptureMouse()) return;

		if (_rel > 0) {
			if (mItemIndex != 0) {
				if (mItemIndex == ITEM_NONE) mItemIndex = 0;
				else mItemIndex --;
				Edit::setCaption(mList->getItemNameAt(mItemIndex));
				mList->setItemSelectedAt(mItemIndex);
				mList->beginToItemAt(mItemIndex);
				eventComboChangePosition(this, mItemIndex);
			}
		}
		else if (_rel < 0) {
			if ((mItemIndex+1) < mList->getItemCount()) {
				if (mItemIndex == ITEM_NONE) mItemIndex = 0;
				else mItemIndex ++;
				Edit::setCaption(mList->getItemNameAt(mItemIndex));
				mList->setItemSelectedAt(mItemIndex);
				mList->beginToItemAt(mItemIndex);
				eventComboChangePosition(this, mItemIndex);
			}
		}
	}

	void ComboBox::notifyMousePressed(WidgetPtr _sender, int _left, int _top, MouseButton _id)
	{
		// обязательно отдаем отцу, а то мы у него в наглую отняли
		Edit::notifyMousePressed(_sender, _left, _top, _id);

		mDropMouse = true;

		// показываем список
		if (mModeDrop) notifyButtonPressed(null, _left, _top, _id);
	}

	void ComboBox::notifyEditTextChange(WidgetPtr _sender)
	{
		// сбрасываем выделенный элемент
		if (ITEM_NONE != mItemIndex) {
			mItemIndex = ITEM_NONE;
			mList->setItemSelectedAt(mItemIndex);
			mList->beginToItemFirst();
			eventComboChangePosition(this, mItemIndex);
		}
	}

	void ComboBox::showList()
	{
		// пустой список не показываем
		if (mList->getItemCount() == 0) return;

		mListShow = true;

		size_t height = mList->getOptimalHeight();
		if (height > mMaxHeight) height = mMaxHeight;

		// берем глобальные координаты выджета
		IntCoord coord = this->getAbsoluteCoord();

		//показываем список вверх
		if ((coord.top + coord.height + height) > (size_t)Gui::getInstance().getViewHeight()) {
			coord.height = height;
			coord.top -= coord.height;
		}
		// показываем список вниз
		else {
			coord.top += coord.height;
			coord.height = height;
		}
		mList->setCoord(coord);

		if (mShowSmooth) {
			ControllerFadeAlpha * controller = new ControllerFadeAlpha(COMBO_ALPHA_MAX, COMBO_ALPHA_COEF, true);
			ControllerManager::getInstance().addItem(mList, controller);
		}
		else mList->show();

		InputManager::getInstance().setKeyFocusWidget(mList);
	}

	void ComboBox::actionWidgetHide(WidgetPtr _widget)
	{
		_widget->hide();
		_widget->setEnabled(true);
	}

	void ComboBox::hideList()
	{
		mListShow = false;

		if (mShowSmooth) {
			ControllerFadeAlpha * controller = new ControllerFadeAlpha(COMBO_ALPHA_MIN, COMBO_ALPHA_COEF, false);
			controller->eventPostAction = newDelegate(this, &ComboBox::actionWidgetHide);
			ControllerManager::getInstance().addItem(mList, controller);
		}
		else mList->hide();
	}

	void ComboBox::setItemSelectedAt(size_t _index)
	{
		MYGUI_ASSERT_RANGE_AND_NONE(_index, mList->getItemCount(), "ComboBox::setItemSelectedAt");
		mItemIndex = _index;
		mList->setItemSelectedAt(_index);
		if (_index == ITEM_NONE)
		{
			Edit::setCaption("");
			return;
		}
		Edit::setCaption(mList->getItemNameAt(_index));
		Edit::updateView(0); // hook for update
	}

	void ComboBox::setItemNameAt(size_t _index, const Ogre::UTFString & _name)
	{
		mList->setItemNameAt(_index, _name);
		mItemIndex = ITEM_NONE;//FIXME
		mList->setItemSelectedAt(mItemIndex);//FIXME
	}

	void ComboBox::setItemDataAt(size_t _index, Any _data)
	{
		mList->setItemDataAt(_index, _data);
		mItemIndex = ITEM_NONE;//FIXME
		mList->setItemSelectedAt(mItemIndex);//FIXME
	}

	void ComboBox::insertItemAt(size_t _index, const Ogre::UTFString & _item, Any _data)
	{
		mList->insertItemAt(_index, _item, _data);
		mItemIndex = ITEM_NONE;//FIXME
		mList->setItemSelectedAt(mItemIndex);//FIXME
	}

	void ComboBox::removeItemAt(size_t _index)
	{
		mList->removeItemAt(_index);
		mItemIndex = ITEM_NONE;//FIXME
		mList->clearItemSelected();//FIXME
	}

	void ComboBox::removeAllItems()
	{
		mItemIndex = ITEM_NONE;//FIXME
		mList->removeAllItems();//FIXME заново созданные строки криво стоят
	}

	void ComboBox::setComboModeDrop(bool _drop)
	{
		mModeDrop = _drop;
		setEditStatic(mModeDrop);
	}

} // namespace MyGUI
