// FileFolderPluginOpen.cpp

#include "StdAfx.h"

#include "Common/StringConvert.h"
#include "Windows/Defs.h"
#include "Windows/FileDir.h"
#include "Windows/FileName.h"
#include "Windows/DLL.h"

#include "IFolder.h"
#include "RegistryAssociations.h"
#include "RegistryPlugins.h"

#include "OpenCallback.h"
#include "PluginLoader.h"

using namespace NWindows;
using namespace NRegistryAssociations;

static int FindPlugin(const CObjectVector<CPluginInfo> &plugins, 
    const UString &pluginName)
{
  for (int i = 0; i < plugins.Size(); i++)
    if (plugins[i].Name.CompareNoCase(pluginName) == 0)
      return i;
  return -1;
}

HRESULT OpenFileFolderPlugin(
    const UString &path, 
    HMODULE *module,
    IFolderFolder **resultFolder, 
    HWND parentWindow)
{
  CObjectVector<CPluginInfo> plugins;
  ReadFileFolderPluginInfoList(plugins);

  CSysString pathSys = GetSystemString(path);
  CSysString extension;
  CSysString name, pureName, dot;

  if(!NFile::NDirectory::GetOnlyName(pathSys, name))
    return E_FAIL;
  NFile::NName::SplitNameToPureNameAndExtension(name, pureName, dot, extension);


  int slashPos = pathSys.ReverseFind('\\');
  CSysString dirPrefix;
  CSysString fileName;
  if (slashPos >= 0)
  {
    dirPrefix = pathSys.Left(slashPos + 1);
    fileName = pathSys.Mid(slashPos + 1);
  }
  else
    fileName = pathSys;

  if (!extension.IsEmpty())
  {
    CExtInfo extInfo;
    if (ReadInternalAssociation(extension, extInfo))
    {
      for (int i = extInfo.Plugins.Size() - 1; i >= 0; i--)
      {
        int pluginIndex = FindPlugin(plugins, extInfo.Plugins[i]);
        if (pluginIndex >= 0)
        {
          const CPluginInfo plugin = plugins[pluginIndex];
          plugins.Delete(pluginIndex);
          plugins.Insert(0, plugin);
        }
      }
    }
  }

  for (int i = 0; i < plugins.Size(); i++)
  {
    const CPluginInfo &plugin = plugins[i];
    CPluginLibrary library;
    CMyComPtr<IFolderManager> folderManager;
    CMyComPtr<IFolderFolder> folder;
    HRESULT result = library.LoadAndCreateManager(
        plugin.FilePath, plugin.ClassID, &folderManager);
    if (result != S_OK)
      continue;

    COpenArchiveCallback *openCallbackSpec = new COpenArchiveCallback;
    CMyComPtr<IProgress> openCallback = openCallbackSpec;
    openCallbackSpec->_passwordIsDefined = false;
    openCallbackSpec->_parentWindow = parentWindow;
    openCallbackSpec->LoadFileInfo(dirPrefix, fileName);

    result = folderManager->OpenFolderFile(path, &folder, openCallback);
    if (result == S_OK)
    {
      *module = library.Detach();
      *resultFolder = folder.Detach();
      return S_OK;
    }
    continue;

    /*
    if (result != S_FALSE)
      return result;
    */
  }
  return S_FALSE;
}