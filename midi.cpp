/*

Simple MIDI Player.

This player is able to play MIDI files using DirectSound API and WinMM library.
DirectSound API can use the Microsoft's software synthesizer built-into the Windows operating system.
WinMM library is able to play MIDI files on external software and hardware synthesizers.

*/

#include <comdef.h>
#include <dmusici.h>
#include <iostream>
#include <vector>
#include <string>
#include <mmsystem.h> // Link with winmm.lib
#include <sstream>

#define APP_NAME "Simple MIDI Player"
#define APP_VER "1.0"

const WCHAR* DIRECT_SOUND_DLL = L"dsound.dll";
const WCHAR* WINDOWS_NT_DLL = L"ntdll.dll";
const WCHAR* WINMM_DLL = L"winmm.dll";
const WCHAR* DLS_FILE_NONE = L"-";

// Global pointers for DirectMusic interfaces
IDirectMusicPerformance8* pPerformance = NULL;
IDirectMusic8* pDirectMusic = NULL;
IDirectMusic* pDirectMusicG = NULL;
IDirectSound8* pDirectSound = NULL;
IDirectSound* pDirectSoundG = NULL;
IDirectMusicLoader8* pLoader = NULL;
IDirectMusicCollection8* pDLSCollection = NULL;
IDirectMusicSegment8* pSegment = NULL;
IDirectMusicPort8* pPort = NULL;
IDirectSoundBuffer* pDSBuffer = nullptr;
BOOL isExternalSynth = FALSE;

// WinMM variables.
std::vector<tagMIDIOUTCAPSA> midiOutDevices;

struct DSDeviceData {
	LPGUID guid;
	std::string name;
};
std::vector<DSDeviceData> dsDeviceData;

struct MidiDeviceData {
	std::string name;
};
std::vector<MidiDeviceData> midiDeviceData;

std::string convertWCharToStdStringWinAPI(const WCHAR* wideString) {
	int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, nullptr, 0, nullptr, nullptr);
	if (bufferSize == 0) {
		throw std::invalid_argument("zero buffer size");
		return "";
	}

	std::string result(bufferSize - 1, '\0'); // -1 to exclude null terminator in size
	WideCharToMultiByte(CP_UTF8, 0, wideString, -1, &result[0], bufferSize, nullptr, nullptr);
	return result;
}

void ShutdownDirectMusic()
{
	if (pSegment)
	{
		pSegment->Unload(pPerformance);
		pSegment->Release();
		pSegment = NULL;
	}
	if (pDSBuffer) {
		pDSBuffer->Release();
		pDSBuffer = NULL;
	}
	if (pPort) {
		pPort->Release();
		pPort = NULL;
	}
	if (pDLSCollection)
	{
		pDLSCollection->Release();
		pDLSCollection = NULL;
	}
	if (pLoader)
	{
		pLoader->Release();
		pLoader = NULL;
	}
	if (pDirectSound) {
		pDirectSound->Release();
		pDirectSound = NULL;
	}
	if (pDirectMusic) {
		pDirectMusic->Release();
		pDirectMusic = NULL;
	}
	if (pPerformance)
	{
		pPerformance->CloseDown();
		pPerformance->Release();
		pPerformance = NULL;
	}
	CoUninitialize();
}

void EnumeratePorts() {
	DMUS_PORTCAPS portCaps;
	DWORD dwIndex = 0;
	HRESULT hr;
	MidiDeviceData dd;

	std::cout << "Available DirectMusic Ports:" << std::endl;

	while (true) {
		// Initialize the size of the structure before calling the method
		portCaps.dwSize = sizeof(DMUS_PORTCAPS);

		// Enumerate the next port
		hr = pDirectMusic->EnumPort(dwIndex, &portCaps);

		if (hr == S_OK) {
			dd.name = convertWCharToStdStringWinAPI(portCaps.wszDescription);
			midiDeviceData.push_back(dd);

			std::wcout << L"[" << dwIndex << L"] " << portCaps.wszDescription << std::endl;

			dwIndex++;
		}
		else if (hr == S_FALSE) {
			// No more ports are available; end the loop
			break;
		}
		else {
			// An error occurred
			std::cerr << "Error during port enumeration: " << hr << std::endl;
			break;
		}
	}
}

BOOL CALLBACK DSEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext)
{
	LPGUID lpTemp = NULL;

	if (lpGUID != NULL)  //  NULL only for "Primary Sound Driver".
	{
		if ((lpTemp = (LPGUID)malloc(sizeof(GUID))) == NULL)
		{
			return(TRUE);
		}
		memcpy(lpTemp, lpGUID, sizeof(GUID));
	}

	//std::cout << lpGUID << " " << lpszDesc << " " << lpszDrvName << std::endl; //DEBUG.
	DSDeviceData dd;
	dd.guid = lpGUID;
	dd.name = std::string(lpszDesc);
	dsDeviceData.push_back(dd);

	free(lpTemp);
	return TRUE;
}

HRESULT EnumerateDirectSoundDevices() {
	std::cout << "Available DirectSound Devices:" << std::endl;

	HRESULT hr = DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, NULL);
	if (FAILED(hr)) return hr;

	for (size_t i = 0; i < dsDeviceData.size(); ++i) {
		std::cout << "[" << i << "] (" << dsDeviceData[i].guid << ") " << dsDeviceData[i].name << std::endl;
	}

	return S_OK;
}

HRESULT CreateMusicPort(DMUS_PORTCAPS portCaps)
{
	DMUS_PORTPARAMS8 portParams;
	ZeroMemory(&portParams, sizeof(portParams));
	portParams.dwSize = sizeof(DMUS_PORTPARAMS8);
	portParams.dwValidParams = 0;

	/*
		Available flags :

			DMUS_PORTPARAMS_VOICES
			DMUS_PORTPARAMS_CHANNELGROUPS
			DMUS_PORTPARAMS_AUDIOCHANNELS
			DMUS_PORTPARAMS_SAMPLERATE
			DMUS_PORTPARAMS_EFFECTS
			DMUS_PORTPARAMS_SHARE

	typedef struct _DMUS_PORTCAPS
	{
		DWORD   dwSize;
		DWORD   dwFlags;
		GUID    guidPort;
		DWORD   dwClass;
		DWORD   dwType;
		DWORD   dwMemorySize;
		DWORD   dwMaxChannelGroups;
		DWORD   dwMaxVoices;
		DWORD   dwMaxAudioChannels;
		DWORD   dwEffectFlags;
		WCHAR   wszDescription[DMUS_MAX_DESCRIPTION];
	} DMUS_PORTCAPS;

	*/

	portParams.dwValidParams |= DMUS_PORTPARAMS_VOICES;
	portParams.dwVoices = portCaps.dwMaxVoices;

	portParams.dwValidParams |= DMUS_PORTPARAMS_CHANNELGROUPS;
	portParams.dwChannelGroups = portCaps.dwMaxChannelGroups;

	portParams.dwValidParams |= DMUS_PORTPARAMS_AUDIOCHANNELS;
	portParams.dwAudioChannels = portCaps.dwMaxAudioChannels;

	portParams.dwValidParams |= DMUS_PORTPARAMS_SAMPLERATE;
	portParams.dwSampleRate = 44100;

	portParams.dwValidParams |= DMUS_PORTPARAMS_EFFECTS;
	portParams.dwEffectFlags = portCaps.dwEffectFlags;

	portParams.dwValidParams |= DMUS_PORTPARAMS_SHARE;
	portParams.fShare = TRUE;

	portParams.dwValidParams |= DMUS_PORTPARAMS_FEATURES;
	portParams.dwFeatures = DMUS_PORT_FEATURE_AUDIOPATH; // DMUS_PORT_FEATURE_STREAMING

	HRESULT hr = pDirectMusic->CreatePort(portCaps.guidPort, &portParams, &pPort, NULL);
	if (FAILED(hr)) return hr;

	// Check ability to use DLS.
	DMUS_PORTCAPS curPortCaps;
	curPortCaps.dwSize = sizeof(DMUS_PORTCAPS);

	hr = pPort->GetCaps(&curPortCaps);
	if (FAILED(hr)) return hr;

	if (curPortCaps.dwFlags & DMUS_PC_DLS) {
		std::cout << "Port supports DLS" << std::endl;
	}
	else {
		std::cout << "Port does not supports DLS" << std::endl;
	}

	if (curPortCaps.dwFlags & DMUS_PC_AUDIOPATH) {
		std::cout << "Port supports Audio Path feature" << std::endl;
	}
	else {
		std::cout << "Port does not support Audio Path feature" << std::endl;
	}

	if (curPortCaps.dwFlags & DMUS_PC_EXTERNAL) {
		isExternalSynth = TRUE;
		std::cout << "Port is an external MIDI module" << std::endl;
	}
	else {
		std::cout << "Port is not an external MIDI module" << std::endl;
	}

	if (curPortCaps.dwFlags & DMUS_PC_SOFTWARESYNTH) {
		std::cout << "Port is a software synthesizer" << std::endl;
	}
	else {
		std::cout << "Port is not a software synthesizer" << std::endl;
	}

	return S_OK;
}

LPCGUID GetDeviceGuidByIndex(int index) {
	if (index < 0) {
		return NULL;
	}

	if (index < int(dsDeviceData.size())) {
		return dsDeviceData[index].guid;
	}

	return NULL;
}

DMUS_PORTCAPS GetPortCapsByIndex(int idx) {
	DMUS_PORTCAPS emptyPortCaps;
	DMUS_PORTCAPS portCaps;
	DWORD dwIndex = 0;
	HRESULT hr;

	while (true) {
		portCaps.dwSize = sizeof(DMUS_PORTCAPS);
		hr = pDirectMusic->EnumPort(dwIndex, &portCaps);

		if (hr == S_FALSE) {
			// Device list is searched, but device is not found
			std::cerr << "Device [" << idx << "] is not found" << std::endl;
			return emptyPortCaps;
		}

		if (hr != S_OK) {
			// An error occurred
			std::cerr << "Error during port enumeration: " << hr << std::endl;
			return emptyPortCaps;
		}

		if (dwIndex != idx)
		{
			dwIndex++;
			continue;
		}

		break;
	}

	return portCaps;
}

HRESULT Initialise(int ds_device_idx, int midi_output_device_idx, WCHAR* dls_file_w)
{
	HWND hWnd = GetConsoleWindow();
	if (hWnd == NULL) {
		std::cerr << "Can not get console window handle" << std::endl;
		return E_FAIL;
	}

	HRESULT hr;

	// Initialize COM
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) return hr;

	// Create the loader object
	hr = CoCreateInstance(CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC, IID_IDirectMusicLoader8, (void**)&pLoader);
	if (FAILED(hr)) return hr;

	// Create the performance object
	hr = CoCreateInstance(CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC, IID_IDirectMusicPerformance8, (void**)&pPerformance);
	if (FAILED(hr)) return hr;

	// Get DirectMusic interface
	hr = CoCreateInstance(CLSID_DirectMusic, NULL, CLSCTX_INPROC, IID_IDirectMusic8, (void**)&pDirectMusic);
	if (FAILED(hr)) return hr;

	// Enumerate DirectSound devices
	EnumerateDirectSoundDevices();
	std::cout << std::endl;

	// Enumerate MIDI output devices
	EnumeratePorts();
	std::cout << std::endl;

	// Create the DirectSound object
	LPCGUID pcGuidDevice = GetDeviceGuidByIndex(ds_device_idx);
	hr = DirectSoundCreate8(pcGuidDevice, &pDirectSound, NULL);
	if (FAILED(hr)) return hr;

	if (ds_device_idx >= 0) {
		std::cout << "Using DirectSound device: " << dsDeviceData[ds_device_idx].name << std::endl;
	}

	hr = pDirectSound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
	if (FAILED(hr)) return hr;

	DSBUFFERDESC bufferDesc = {};
	bufferDesc.dwSize = sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN;

	hr = pDirectSound->CreateSoundBuffer(&bufferDesc, &pDSBuffer, NULL);
	if (FAILED(hr)) return hr;

	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = 2;
	waveFormat.nSamplesPerSec = 44100;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8; // = nChannels * (wBitsPerSample / 8).
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign; // = nSamplesPerSec * nBlockAlign.
	waveFormat.cbSize = 0;

	hr = pDSBuffer->SetFormat(&waveFormat);
	if (FAILED(hr)) return hr;

	hr = pDirectMusic->SetDirectSound(pDirectSound, hWnd);
	if (FAILED(hr)) return hr;

	// Load DLS file.
	if (wcscmp(dls_file_w, DLS_FILE_NONE) != 0) {
		hr = pLoader->LoadObjectFromFile(CLSID_DirectMusicCollection, IID_IDirectMusicCollection8, dls_file_w, (void**)&pDLSCollection);
		if (FAILED(hr)) return hr;
	}

	hr = pDirectMusic->QueryInterface(IID_IDirectMusic, (void**)&pDirectMusicG);
	if (FAILED(hr)) return hr;
	hr = pDirectSound->QueryInterface(IID_IDirectSound, (void**)&pDirectSoundG);
	if (FAILED(hr)) return hr;

	if (midi_output_device_idx < 0) {

		/*
		The DMUS_APATH types are used in conjunction with CreateStandardAudioPath to
		build default path types. _SHARED_ means the same buffer is shared across multiple
		instantiations of the audiopath type. _DYNAMIC_ means a unique buffer is created
		every time.

		#define DMUS_APATH_SHARED_STEREOPLUSREVERB   1       // A standard music set up with stereo outs and reverb.
		#define DMUS_APATH_DYNAMIC_3D                6       // An audio path with one dynamic bus from the synth feeding to a dynamic 3d buffer. Does not send to env reverb.
		#define DMUS_APATH_DYNAMIC_MONO              7       // An audio path with one dynamic bus from the synth feeding to a dynamic mono buffer.
		#define DMUS_APATH_DYNAMIC_STEREO            8       // An audio path with two dynamic buses from the synth feeding to a dynamic stereo buffer.
		*/
		DWORD dwDefaultPathType = DMUS_APATH_SHARED_STEREOPLUSREVERB;
		// DMUS_APATH_SHARED_STEREOPLUSREVERB - sounds flat.
		// DMUS_APATH_DYNAMIC_3D - sounds flat and quiet.
		// DMUS_APATH_DYNAMIC_MONO - sounds flat.
		// DMUS_APATH_DYNAMIC_STEREO - sounds flat and quiet.

		DWORD dwPChannelCount = 16;
		DWORD dwFlags = NULL;
		hr = pPerformance->InitAudio(&pDirectMusicG, &pDirectSoundG, hWnd, dwDefaultPathType, dwPChannelCount, dwFlags, NULL); // Not compatible with AddPort !
		if (FAILED(hr)) return hr;
	}
	else {
		// Find an output device by its index
		DMUS_PORTCAPS portCaps = GetPortCapsByIndex(midi_output_device_idx);

		std::cout << "Using MIDI device: " << midiDeviceData[midi_output_device_idx].name << std::endl;

		// Create a port and activate it
		hr = CreateMusicPort(portCaps);
		if (FAILED(hr)) return hr;

		hr = pPerformance->Init(&pDirectMusicG, pDirectSoundG, hWnd); // Old method, compatible with AddPort.
		if (FAILED(hr)) return hr;

		hr = pPerformance->AddPort(pPort);
		if (FAILED(hr)) return hr;

		hr = pPort->Activate(TRUE);
		if (FAILED(hr)) return hr;
	}

	return S_OK;
}

HRESULT ListDevices()
{
	HRESULT hr;

	// Initialize COM
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) return hr;

	// Create the performance object
	hr = CoCreateInstance(CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC, IID_IDirectMusicPerformance8, (void**)&pPerformance);
	if (FAILED(hr)) return hr;

	// Get DirectMusic interface
	hr = CoCreateInstance(CLSID_DirectMusic, NULL, CLSCTX_INPROC, IID_IDirectMusic8, (void**)&pDirectMusic);
	if (FAILED(hr)) return hr;

	// Enumerate DirectSound devices
	EnumerateDirectSoundDevices();
	std::cout << std::endl;

	// Enumerate MIDI output devices
	EnumeratePorts();
	std::cout << std::endl;

	return S_OK;
}

HRESULT PlayMidi(WCHAR* midi_file_w, BOOL isExternalSynth)
{
	HRESULT hr;

	if (isExternalSynth) {
		// It looks like Microsoft has cut all the functions for communicating with external synthesizers.
		// This is very sad.
		return E_FAIL;
	}
	else {
		// Load MIDI file.
		hr = pLoader->LoadObjectFromFile(CLSID_DirectMusicSegment, IID_IDirectMusicSegment8, midi_file_w, (void**)&pSegment);
		if (FAILED(hr)) return hr;

		// Download instrument data to the synth (DLS)
		hr = pSegment->Download(pPerformance);
		if (FAILED(hr)) return hr;

		if (pPerformance && pSegment) {
			// 5. Play the segment
			hr = pPerformance->PlaySegment(pSegment, DMUS_SEGF_AFTERPREPARETIME, 0, NULL);
			if (FAILED(hr)) return hr;

			/*
			pPerformance->PlaySegmentEx(
				pSegment,     // Segment to play
				NULL,         // Optional segment to fade from
				NULL,         // Optional start time
				DMUS_SEGF_BEAT, // Play on the next beat (standard MIDI files usually start immediately)
				0,            // Start time (if using DMUS_SEGF_MEASURE/BEAT/GRID)
				NULL,         // Object to receive notification

				// ?
				NULL,         // Audiopath (NULL uses default path set in InitAudio)

				NULL          // Performance (for secondary performances)
			);
			*/
		}
	}

	return S_OK;
}

void print_result(HRESULT hr)
{
	_com_error err(hr);
	LPCTSTR errMsg = err.ErrorMessage();

	std::cerr << "Result: " << hr << "; Error: " << errMsg << std::endl;
}

void PrintWin32Error(const std::string& functionName) {
	DWORD errorCode = GetLastError(); // Get the last error code
	if (errorCode == 0)
	{
		std::cerr << "No error reported by " << functionName << std::endl;
		return;
	}

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&messageBuffer,
		0,
		NULL
	);

	if (size > 0 && messageBuffer != nullptr) {
		std::cerr << "Error in " << functionName << ": " << messageBuffer << std::endl;
		LocalFree(messageBuffer); // Free the allocated buffer
	}
	else {
		std::cerr << "Error in " << functionName << ": Unknown error code " << errorCode << std::endl;
	}
}

// Link with Version.lib.
std::wstring GetLibraryVersion(WCHAR* dllFileName) {
	WCHAR systemFolderPath[MAX_PATH];
	// dsound.dll is typically located in the System32 directory
	// GetSystemDirectory is a reliable way to get this path
	if (GetSystemDirectoryW(systemFolderPath, MAX_PATH) == 0) {
		PrintWin32Error("GetSystemDirectoryW");
		return L"";
	}

	std::wstring dllFilePath = std::wstring(systemFolderPath) + L"\\" + dllFileName;
	DWORD dwHandle = 0;
	DWORD dwSize = GetFileVersionInfoSizeW(dllFilePath.c_str(), &dwHandle);
	if (dwSize == 0) {
		PrintWin32Error("GetFileVersionInfoSizeW");
		return L"";
	}

	std::unique_ptr<BYTE[]> versionInfo(new BYTE[dwSize]);
	if (!GetFileVersionInfoW(dllFilePath.c_str(), 0, dwSize, versionInfo.get())) {
		PrintWin32Error("GetFileVersionInfoW");
		return L"";
	}

	VS_FIXEDFILEINFO* pFileInfo = nullptr;
	UINT puLenFileInfo = 0;
	if (!VerQueryValueW(versionInfo.get(), L"\\\\", (LPVOID*)&pFileInfo, &puLenFileInfo)) {
		PrintWin32Error("VerQueryValueW");
		return L"";
	}

	if (!pFileInfo) {
		std::cerr << "Version information is not found";
		return L"";
	}

	DWORD dwFileVersionMS = pFileInfo->dwFileVersionMS;
	DWORD dwFileVersionLS = pFileInfo->dwFileVersionLS;

	// Extract the major, minor, revision, and build numbers
	DWORD major = HIWORD(dwFileVersionMS);
	DWORD minor = LOWORD(dwFileVersionMS);
	DWORD revision = HIWORD(dwFileVersionLS);
	DWORD build = LOWORD(dwFileVersionLS);

	wchar_t versionStr[256];
	swprintf_s(versionStr, L"%u.%u.%u.%u", major, minor, revision, build);
	return versionStr;
}

void ListMidiOutDevicesWithWinmm() {
	std::cout << "Available MIDI Out Devices:" << std::endl;

	int midiOutDevCount = midiOutGetNumDevs(); // Get count
	for (int i = 0; i < midiOutDevCount; i++) {
		tagMIDIOUTCAPSA caps;
		midiOutGetDevCapsA(i, &caps, sizeof(tagMIDIOUTCAPSA));
		midiOutDevices.push_back(caps);
	}

	for (int i = 0; i < midiOutDevices.size(); i++) {
		std::cout << "[" << i << "] (" << midiOutDevices[i].wPid << ") " <<
			midiOutDevices[i].szPname <<
			" Drv=" << midiOutDevices[i].vDriverVersion <<
			" PID=" << midiOutDevices[i].wPid <<
			" MID=" << midiOutDevices[i].wMid <<
			" DevType=" << midiOutDevices[i].wTechnology <<
			" Voices=" << midiOutDevices[i].wVoices <<
			" ChanMask=" << midiOutDevices[i].wChannelMask <<
			" Funcs=" << midiOutDevices[i].dwSupport <<
			std::endl;
	}

	std::cout << std::endl;
}

void testDefaultMidiOut() {
	HMIDIOUT hMidiOut;
	MMRESULT result;

	// Open the default MIDI output device
	result = midiOutOpen(&hMidiOut, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
	if (result != MMSYSERR_NOERROR) {
		// Handle error
		std::cerr << result;
		return;
	}

	// Prepare a Note On message (Channel 0, Note C4 (60), Velocity 127)
	DWORD dwMsg = 0x007F3C90; // 0x90 (Note On), 0x3C (Note C4), 0x7F (Velocity 127)

	// Send the Note On message
	result = midiOutShortMsg(hMidiOut, dwMsg);
	if (result != MMSYSERR_NOERROR) {
		// Handle error
		std::cerr << result;
		return;
	}

	// Wait for a short duration (e.g., 1 second)
	Sleep(1000);

	// Prepare a Note Off message (Channel 0, Note C4 (60), Velocity 0)
	dwMsg = 0x00003C80; // 0x80 (Note Off), 0x3C (Note C4), 0x00 (Velocity 0)

	// Send the Note Off message
	result = midiOutShortMsg(hMidiOut, dwMsg);
	if (result != MMSYSERR_NOERROR) {
		// Handle error
		std::cerr << result;
		return;
	}

	// Close the MIDI output device
	midiOutClose(hMidiOut);

	return;
}

int playMidiWithWinmm(int midi_output_device_idx, char* midi_file)
{
	HWND hWnd = GetConsoleWindow();
	if (hWnd == NULL) {
		std::cerr << "Can not get console window handle" << std::endl;
		return 1;
	}

	std::string cmd;
	std::string midiFileName = midi_file;
	int portNumber = midi_output_device_idx; // Device ID, MIDI Out device index in the list.

	MCIERROR err;

	// 1. Open the MIDI file and give it an alias (e.g., "music")
	cmd = "open \"" + midiFileName + "\" type sequencer alias music";
	std::cout << "> " << cmd << std::endl;
	err = mciSendString(cmd.c_str(), NULL, 0, NULL);
	if (err != 0) {
		std::cerr << "ERROR";
		return 1;
	}

	// 2. Set the output port (device ID varies from 0 to midiOutGetNumDevs()-1)
	// The port number (portNumber) is the device ID
	std::ostringstream oss;
	oss << portNumber;
	cmd = "set music port " + oss.str();
	std::cout << "> " << cmd << std::endl;
	err = mciSendString(cmd.c_str(), NULL, 0, NULL);
	if (err != 0) {
		std::cerr << "ERROR";
		return 1;
	}

	// 3. Play the file
	cmd = "play music notify"; // 'notify' uses the window handle for completion messages
	std::cout << "> " << cmd << std::endl;
	err = mciSendString(cmd.c_str(), NULL, 0, hWnd);
	if (err != 0) {
		std::cerr << "ERROR";
		return 1;
	}

	std::cout << "Playing MIDI file: " << midi_file << std::endl;
	std::cout << "Press Enter to stop ..." << std::endl;
	std::cin.get();

	// When done, close the device
	cmd = "close music";
	std::cout << "> " << cmd << std::endl;
	err = mciSendString(cmd.c_str(), NULL, 0, NULL);
	if (err != 0) {
		std::cerr << "ERROR";
		return 1;
	}
}

int main(int argc, char* argv[])
{
	std::cout << APP_NAME << " " << APP_VER << std::endl;
	std::wstring directSoundApiVersion = GetLibraryVersion(const_cast<wchar_t*>(DIRECT_SOUND_DLL));
	std::wcout << "DirectSound API:\t" << DIRECT_SOUND_DLL << " version: " << directSoundApiVersion << std::endl;
	std::wstring winmmVersion = GetLibraryVersion(const_cast<wchar_t*>(WINMM_DLL));
	std::wcout << "WinMM:\t\t\t" << WINMM_DLL << " version: " << winmmVersion << std::endl;
	std::wstring windowsNtVersion = GetLibraryVersion(const_cast<wchar_t*>(WINDOWS_NT_DLL));
	std::wcout << "Windows:\t\t" << WINDOWS_NT_DLL << " version: " << windowsNtVersion << std::endl;
	std::cout << std::endl;

	HRESULT hr;

	if (argc <= 1) // No arguments.
	{
		std::cout << "Usage: " << std::endl;
		std::cout << "\t<executable> <Work mode> ..." << std::endl;
		std::cout << std::endl;

		std::cout << "Number of arguments depends on the work mode set as a first argument in the command line." << std::endl;
		std::cout << "Available word modes are: " << std::endl;
		std::cout << "\t DS - This mode uses DirectSound API;" << std::endl;
		std::cout << "\t MM - This mode uses WinMM library." << std::endl;
		std::cout << std::endl;

		std::cout << "Arguments (4) for DirectSound mode are: " << std::endl;
		std::cout << "\t<DirectSound device index> <MIDI output device index> <DLS file> <MIDI file>" << std::endl;
		std::cout << "Arguments (2) for WinMM mode are: " << std::endl;
		std::cout << "\t<Port number / Device ID> <MIDI file>" << std::endl;
		std::cout << std::endl;

		std::cout << "Notes for DirectSound mode: " << std::endl;
		std::cout << "\tSet the DirectSound device index to a negative value to use the default device." << std::endl;
		std::cout << "\tSet the MIDI output device index to a negative value to use the default device." << std::endl;
		std::cout << "\tTo disable loading DLS, use the '" << convertWCharToStdStringWinAPI(DLS_FILE_NONE) << "' as DLS file." << std::endl;
		std::cout << "\tIn the past time DirectSound API used to support great functionality, such as EAX, 3D positional audio and many other features. " <<
			"Unfortunately, Microsoft corporation destroyed the whole API and thousands and millions of hours of many people's work when Windows Vista came out. " <<
			"In its current state, DirectSound API does not really work with any MIDI synthesizers except the one built-into the Windows operating system. " <<
			std::endl;
		std::cout << std::endl;

		std::cout << "Notes for WinMM mode: " << std::endl;
		std::cout << "\tDo not use this mode for playing MIDI files on a Microsoft's software synthesizer, also known as Microsoft GS Wavetable Synth. " <<
			"This mode is used mostly for software and hardware synthesizers present on your sound card or for external hardware synthesizers. " << std::endl;
		std::cout << std::endl;

		std::cout << "Examples: " << std::endl;
		std::cout << "\ttool.exe DS -1 -1 gm.dls music.mid" << std::endl;
		std::cout << "\ttool.exe DS -1 -1 - music.mid" << std::endl;
		std::cout << "\ttool.exe MM 1 music.mid" << std::endl;
		std::cout << std::endl;

		ListMidiOutDevicesWithWinmm();

		hr = ListDevices();
		if (FAILED(hr))
		{
			std::cerr << "Failed to list devices." << std::endl;
			print_result(hr);
			ShutdownDirectMusic();
			return 1;
		}

		ShutdownDirectMusic();
		return 0;
	}

	std::string workModeStr = argv[1]; // Work mode selector

	int midi_output_device_idx;
	char* midi_file;

	if (workModeStr == "DS")
	{
		if (argc <= 1 + 4) {
			std::cerr << "Arguments are not set." << std::endl;
			return 1;
		}

		char* ds_device_index_str = argv[1 + 1]; // Index of a DirectSound output device, starting from 0
		char* midi_output_device_index_str = argv[1 + 2]; // Index of a MIDI output device, starting from 0
		char* dls_file = argv[1 + 3]; // DLS file
		midi_file = argv[1 + 4]; // MIDI file

		int ds_device_idx = std::atoi(ds_device_index_str);
		midi_output_device_idx = std::atoi(midi_output_device_index_str);

		WCHAR dls_file_w[MAX_PATH];
		MultiByteToWideChar(CP_ACP, 0, dls_file, -1, dls_file_w, MAX_PATH);
		WCHAR midi_file_w[MAX_PATH];
		MultiByteToWideChar(CP_ACP, 0, midi_file, -1, midi_file_w, MAX_PATH);

		hr = Initialise(ds_device_idx, midi_output_device_idx, dls_file_w);
		if (FAILED(hr))
		{
			std::cerr << "DirectMusic failed to initialise." << std::endl;
			print_result(hr);
			ShutdownDirectMusic();
			return 1;
		}

		std::cout << "Playing MIDI file: " << midi_file << std::endl;
		std::cout << "Press Enter to stop ..." << std::endl;
		hr = PlayMidi(midi_file_w, isExternalSynth);
		if (FAILED(hr))
		{
			std::cerr << "Failed to play MIDI file." << std::endl;
			print_result(hr);
			ShutdownDirectMusic();
			return 2;
		}

		std::cin.get();
		ShutdownDirectMusic();
		return 0;
	}
	else if (workModeStr == "MM")
	{
		if (argc <= 1 + 2)
		{
			std::cerr << "Arguments are not set." << std::endl;
			return 1;
		}

		char* midi_output_device_index_str = argv[1 + 1]; // Index of a MIDI output device, starting from 0
		midi_file = argv[1 + 2]; // MIDI file

		midi_output_device_idx = std::atoi(midi_output_device_index_str);

		ListMidiOutDevicesWithWinmm();

		int err = playMidiWithWinmm(midi_output_device_idx, midi_file);
		if (err != 0) {
			return err;
		}

		return 0;
	}

	std::cerr << "Unknown work mode: " << workModeStr << std::endl;
	return 1;
}
