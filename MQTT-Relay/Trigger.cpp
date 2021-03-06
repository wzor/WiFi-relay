#include "Trigger.h"
#include "MQTTprocess.h"
#include "MQTTswitch.h"
#include <TimeLib.h>
#include <FS.h>
#include "Variable.h"
#include "Json.h"

static int _sort;
static int _uid;
static Trigger * _firstTrigger = nullptr;
static Trigger * _lastTrigger = nullptr;
static Trigger * _currentProcesing = nullptr;

Trigger::Trigger()
{
	_uid++;
	_sort++;
	uid = _uid;
	sort = _sort;
	next = nullptr;
}

Trigger::~Trigger()
{
}

void Trigger::Register()
{
	//Serial.println("register 1");
	if (_lastTrigger == nullptr) {
		//Serial.println("register 2");
		_firstTrigger = this;
		_firstTrigger->next = nullptr;
		_lastTrigger = this;
	}
	else {
		_lastTrigger->next = this;
		_lastTrigger = this;
	}
	if (_uid <= uid) _uid = uid;

	Serial.printf("Trigger %i - %s registered\n", uid, type);
}

void Trigger::Unregister()
{
	Trigger * t = _firstTrigger;
	Trigger * prev = nullptr;
	while (t != nullptr) {
		if (t == this) {
			if (prev == nullptr) {
				_firstTrigger = this->next;
			}
			else {
				prev->next = this->next;
			}
			t = nullptr;
		}
		else {
			prev = t;
			t = t->next;
		}
	}
	if (_currentProcesing == this) _currentProcesing = this->next;
}

void Trigger::printInfo(JsonString * ret, bool detailed)
{
	ret->AddValue("name", name);
	ret->AddValue("uid", String(uid));
	ret->AddValue("days", String(days));
	if (detailed) {
		ret->AddValue("type", type);
		ret->AddValue("template", type);
		ret->AddValue("editingtemplate", String(type) + "edit");
	}
}

bool Trigger::save()
{
	Serial.println("Trigger save...");
	String num = String(uid);
	if (num.length() < 2) num = "0" + num;

	String fileName = "/config/" + proc->name + "/" + num + String(type) + ".json";
	Serial.println(fileName.c_str());
	File f = SPIFFS.open(fileName, "w");
	JsonString ret = "";
	printInfo(&ret, false);
	f.print(ret);
	f.flush();
	f.close();
	Serial.println("Saved OK.");
	return true;
}

Trigger * Trigger::getFirstTrigger()
{
	return _firstTrigger;
}

Trigger * Trigger::getLastTrigger()
{
	return _lastTrigger;
}

void Trigger::processNext(time_t * time)
{
	if (timeStatus() == timeNotSet) return;
	if (_currentProcesing == nullptr) { _currentProcesing = _firstTrigger; }
	if (_currentProcesing != nullptr) {
		_currentProcesing->loop(time);
		_currentProcesing = _currentProcesing->next;
	}
}

void Trigger::loadConfig(MQTTswitch * proc)
{
	Serial.printf("Loding %s config\n", proc->name.c_str());
	String dirName = "/config/" + proc->name;
	Dir dir = SPIFFS.openDir(dirName);
	Trigger * t = nullptr;

	while (dir.next()) {
		Serial.println(dir.fileName());
		if (dir.fileSize()) {
			File f = dir.openFile("r");
			String fName = String(f.name()).substring(dirName.length());
			String fNum = fName.substring(1, 3);
			fName = fName.substring(3);
			Serial.printf("config found num=%s name=%s \n", fNum.c_str(), fName.c_str());

			if (fName.startsWith("onoff")) {
				t = new OnOffTrigger();
			}
			else if (fName.startsWith("pwm")) {
				t = new PWMTrigger();
			}
			else if (fName.startsWith("termo")) {
				t = new Termostat();
			}
			else if (fName.startsWith("vent")) {
				t = new Venting();
			}
			if (t != nullptr) {
				t->uid = fNum.toInt();
				t->load(&f);
				t->proc = proc;
				t->Register();
			}
			f.close();
		}
	}


}

int Trigger::generateNewUid()
{
	int ret = 1;
	int maxUid = 0;
	Trigger * t = _firstTrigger;
	while (t != nullptr) {
		if (t->uid > maxUid) maxUid = t->uid;
		if (t->uid == ret) {
			ret++;
			t = _firstTrigger;
		}
		else {
			t = t->next;
		}
	}
	if (_uid != maxUid) {
		Serial.printf("Max UID reseed %i => %i", _uid, maxUid);
		_uid = maxUid;
	}
	return ret;
}

void Trigger::Sort()
{
	/*Trigger * triggers[99];
	Trigger * t = _firstTrigger;
	int i = 0;
	while (t != nullptr) {
		triggers[i] = t;
		i++;
		t = t->next;
	}
	qsort(triggers, i, sizeof(Trigger *), Compare);
	for (int j = 0; j < i; j++) {
		if (j == 0) {
			_firstTrigger = triggers[j];
			_lastTrigger = triggers[j];
		}
		else {
			triggers[j - 1]->next = triggers[j];
			_lastTrigger = triggers[j];
		}
	}
	if (_lastTrigger != nullptr) _lastTrigger->next = nullptr;
	*/
	Serial.println("Sorting triggers...");
	//�� ����� �������� ������ ���� ������� � ��������� �������
	//ϳ��� ���������� �� ����� ����������� (��������� ������������ �������)
	Trigger * t = nullptr;
	Trigger * n = nullptr;//next
	Trigger * p = nullptr;//prev
	bool was_swap = true;//���� ������� ������� ������ �������� ��������
	while (true) {
		if (t == nullptr) {//�������� � ������� ��������
			if (was_swap) {
				t = _firstTrigger;
				p = nullptr;
				was_swap = false;
			}
			else {
				//�� ��������� �������� �������� �� ��������������, ������� ����� �����������
				return;
			}
		}
		if (t != nullptr) {
			n = t->next;
			if (n != nullptr) {//� ��������� �������, ������� �� �� �� ������� ����
				if (t->getSort() > n->getSort()) {//t => ����� ��������� ��� n, ���� ������ ��� n
					was_swap = true;//������ ������ ��� ��������
					t->next = n->next;
					n->next = t;
					if (p != nullptr)
						p->next = n;
					else
						_firstTrigger = n;
					p = n;
				}
				else {
					p = t;
					t = t->next;
				}
			}
			else {
				t = nullptr;//ʳ���� ������, �������� � �������
			}
		}
	}
}

int Trigger::Compare(const void * a, const void * b)
{
	Trigger * ta = (Trigger *)a;
	Trigger * tb = (Trigger *)b;

	int ai = ta->getSort();
	int bi = tb->getSort();
	if (ai > bi)
		return 1;
	else if (ai < bi)
		return -1;
	return 0;
}



OnOffTrigger::OnOffTrigger() :Trigger()
{
	type = "onoff";
}

OnOffTrigger::~OnOffTrigger()
{
}

void OnOffTrigger::loop(time_t * time)
{
	if (year(lastFire) == year(*time) && month(lastFire) == month(*time) && day(lastFire) == day(*time)) return;//���� �������� ��� ���������, �� ������ �� ������

	int d = weekday(*time) - 1;
	if (days & (1 << d)) {//���� ������ ���������
		unsigned int t = (unsigned int)hour(*time) * 60UL + (unsigned int)minute(*time);//������ �� ������� ����
		//Serial.printf("t=%i, time=%i\n", t, this->time);
		if (t >= this->time) {
			if (proc != nullptr) {
				proc->setState(action);
			}
			Serial.printf("Trigger %i - %s Fire\n", uid, type);
			lastFire = *time;
		}
	}
}

void OnOffTrigger::load(File * f) {
	Trigger::load(f);
	JsonString s = JsonString(f->readString());
	name = s.getValue("name");
	days = (unsigned char)(s.getValue("days").toInt());
	time = s.getValue("time").toInt();
	if (s.getValue("action") == "1") {
		action = HIGH;
	}
	else {
		action = LOW;
	}
}

void OnOffTrigger::printInfo(JsonString * ret, bool detailed)
{
	Trigger::printInfo(ret, detailed);
	ret->AddValue("time", String(time));
	if (detailed) {
		if (action == HIGH)
			ret->AddValue("action", "on");
		else
			ret->AddValue("action", "off");
	}
	else {
		if (action == HIGH)
			ret->AddValue("action", "1");
		else
			ret->AddValue("action", "0");
	}

}

PWMTrigger::PWMTrigger()
{
	type = "pwm";
	lastFire = 0;
	stage = 0;
}

PWMTrigger::~PWMTrigger()
{
}

void PWMTrigger::loop(time_t * time)
{

	int d = weekday(*time) - 1;
	if (days & (1 << d)) {//���� ������ ���������
		unsigned long deltaT = *time - lastFire;
		unsigned int t = (unsigned int)hour(deltaT) * 60UL + (unsigned int)minute(deltaT);
		//Serial.println(t);
		if (t < onlength) {
			if (stage == 0) {
				stage++;
				if (!proc->isOn()) {
					Serial.printf("Trigger %i - %s ON\n", uid, type);
					proc->setState(true);
				}
			}
		}
		else if (t < (onlength + offlength))
		{
			if (stage == 1) {
				stage++;
				if (proc->isOn()) {
					Serial.printf("Trigger %i - %s OFF\n", uid, type);
					proc->setState(false);
				}
			}
		}
		else {
			Serial.printf("Trigger %i - %s new Period\n", uid, type);
			lastFire = *time;
			stage = 0;
		}
	}
}

void PWMTrigger::load(File * f)
{
	Trigger::load(f);
	JsonString s = JsonString(f->readString());
	name = s.getValue("name");
	days = (unsigned char)(s.getValue("days").toInt());
	onlength = s.getValue("onlength").toInt();
	offlength = s.getValue("offlength").toInt();
}

void PWMTrigger::printInfo(JsonString * ret, bool detailed)
{
	Trigger::printInfo(ret, detailed);
	ret->AddValue("onlength", String(onlength));
	ret->AddValue("offlength", String(offlength));
}

Termostat::Termostat()
{
	type = "termo";
}

void Termostat::printInfo(JsonString * ret, bool detailed)
{
	Trigger::printInfo(ret, detailed);
	ret->AddValue("start", String(start));
	ret->AddValue("end", String(end));
	ret->AddValue("variable", variable);
	ret->AddValue("min", String(min));
	ret->AddValue("max", String(max));
}

void Termostat::load(File * f)
{
	Trigger::load(f);
	JsonString s = JsonString(f->readString());
	name = s.getValue("name");
	days = (unsigned char)(s.getValue("days").toInt());
	start = s.getValue("start").toInt();
	end = s.getValue("end").toInt();
	variable = s.getValue("variable");
	min = s.getValue("min").toInt();
	max = s.getValue("max").toInt();
}

void Termostat::loop(time_t * time)
{
	int d = weekday(*time) - 1;
	if (days == 127 || days & (1 << d)) {//���� ������ ���������
		unsigned int t = (unsigned int)hour(*time) * 60UL + (unsigned int)minute(*time);
		if ((start == end/*����� ����*/) || (t >= start && t <= end)) {//��� ���������
			float v = Variable::getValue(variable);
			if (v < min) {//����������� ����� ����� �������
				if (!proc->isOn()) {
					Serial.printf("Trigger %i - %s Temperature too low => ON\n", uid, type);
					proc->setState(true);
					state = true;
				}
			}
			else {
				if (v > max) {//����������� ���� ���������
					if (proc->isOn()) {
						Serial.printf("Trigger %i - %s Temperature too high => OFF\n", uid, type);
						proc->setState(false);
						state = false;
					}
				}
			}
		}
		else if (start > end) { //������� ����� ����. ��������� � 22:00 �� 05:00
			if (t >= start || t <= end) {
				float v = Variable::getValue(variable);
				if (v < min) {//����������� ����� ����� �������
					if (!proc->isOn()) {
						Serial.printf("Trigger %i - %s Temperature too low => ON\n", uid, type);
						proc->setState(true);
						state = true;
					}
				}
				else {
					if (v > max) {//����������� ���� ���������
						if (proc->isOn()) {
							Serial.printf("Trigger %i - %s Temperature too high => OFF\n", uid, type);
							proc->setState(false);
							state = false;
						}
					}
				}
			}
		}
		else {
			if (state) {
				if (proc->isOn()) {
					Serial.printf("Trigger %i - %s OFF\n", uid, type);
					proc->setState(false);
					state = false;
				}
			}
		}
	}
}

Venting::Venting()
{
	type = "vent";
}

void Venting::printInfo(JsonString * ret, bool detailed)
{
	Trigger::printInfo(ret, detailed);
	ret->AddValue("start", String(start));
	ret->AddValue("end", String(end));
	ret->AddValue("variable", variable);
	ret->AddValue("min", String(min));
	ret->AddValue("max", String(max));
}

void Venting::load(File * f)
{
	Trigger::load(f);
	JsonString s = JsonString(f->readString());
	name = s.getValue("name");
	days = (unsigned char)(s.getValue("days").toInt());
	start = s.getValue("start").toInt();
	end = s.getValue("end").toInt();
	variable = s.getValue("variable");
	min = s.getValue("min").toInt();
	max = s.getValue("max").toInt();
}

void Venting::loop(time_t * time)
{
	int d = weekday(*time) - 1;
	if (days == 127 || days & (1 << d)) {//���� ������ ���������
		unsigned int t = (unsigned int)hour(*time) * 60UL + (unsigned int)minute(*time);
		if ((start == end/*����� ����*/) || (t >= start && t <= end)) {//��� ���������
			float v = Variable::getValue(variable);
			if (v < min) {//�������� ����� ����� �������
				if (state/*proc->isOn()*/) {
					Serial.printf("Trigger %i - %s Humidity too low => OFF\n", uid, type);
					proc->setState(false);
					state = false;
				}
			}
			else {
				if (v > max) {//�������� ���� ���������
					if (!state/*!proc->isOn()*/) {
						Serial.printf("Trigger %i - %s Humidity too high => ON\n", uid, type);
						proc->setState(true);
						state = true;
					}
				}
			}
		}
		else if (start > end) { //������� ����� ����. ��������� � 22:00 �� 05:00
			if (t >= start || t <= end) {
				float v = Variable::getValue(variable);
				if (v < min) {//�������� ����� ����� �������
					if (state/*proc->isOn()*/) {
						Serial.printf("Trigger %i - %s Humidity too low => OFF\n", uid, type);
						proc->setState(false);
						state = false;
					}
				}
				else {
					if (v > max) {//�������� ���� ���������
						if (!state/*!proc->isOn()*/) {
							Serial.printf("Trigger %i - %s Temperature too high => ON\n", uid, type);
							proc->setState(true);
							state = true;
						}
					}
				}
			}
		}
		/*else {
			if (state) {//TODO: ??? �� ����� ���� �� ������� ???
				if (proc->isOn()) {
					Serial.printf("Trigger %i - %s OFF\n", uid, type);
					proc->setState(false);
					state = false;
				}
			}
		}*/
	}
}

TimeoutTrigger::TimeoutTrigger()
{
	type = "timeout";
}

void TimeoutTrigger::loop(time_t * time)
{
	//if (year(lastFire) == year(*time) && month(lastFire) == month(*time) && day(lastFire) == day(*time)) return;//���� �������� ��� ���������, �� ������ �� ������

	int d = weekday(*time) - 1;
	if (days & (1 << d)) {//���� ������ ���������
		if (proc != nullptr) {

			if (action == LOW && proc->isOn() && startTime == 0) {//���� �������� � ��������� ��������
				startTime = millis();
			}
			else if (action == HIGH && !proc->isOn() && startTime == 0) {//���� �������� � ��������� ��������
				startTime = millis();
			}
			else if (startTime > 0) {//���� ������ ��������
				if ((millis() - startTime) > (len * 60000UL)) {
					proc->setState(action);
					Serial.printf("Trigger %i - %s Fire\n", uid, type);
					startTime = 0;
				}
			}
		}
	}
}

void TimeoutTrigger::load(File * f)
{
	Trigger::load(f);
	JsonString s = JsonString(f->readString());
	name = s.getValue("name");
	days = (unsigned char)(s.getValue("days").toInt());
	len = s.getValue("len").toInt();
	if (s.getValue("action") == "1") {
		action = HIGH;
	}
	else {
		action = LOW;
	}
}

void TimeoutTrigger::printInfo(JsonString * ret, bool detailed)
{
	Trigger::printInfo(ret, detailed);
	ret->AddValue("len", String(len));
	if (detailed) {
		if (action == HIGH)
			ret->AddValue("action", "on");
		else
			ret->AddValue("action", "off");
	}
	else {
		if (action == HIGH)
			ret->AddValue("action", "1");
		else
			ret->AddValue("action", "0");
	}
}
