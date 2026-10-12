#include "fields.h"

const FieldState FieldStates[FieldCount] =
{
    { CPFS_DISPLAY_IN_BOTH,          CPFIS_NONE    },
    { CPFS_DISPLAY_IN_BOTH,          CPFIS_NONE    },
    { CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE    },
    { CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_FOCUSED },
    { CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE    },
};

const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR FieldDescriptors[FieldCount] =
{
    { TileImageField, CPFT_TILE_IMAGE,    const_cast<PWSTR>(L"Image"),     CPFG_CREDENTIAL_PROVIDER_LOGO  },
    { LabelField,     CPFT_LARGE_TEXT,    const_cast<PWSTR>(L"Téléphone"), CPFG_CREDENTIAL_PROVIDER_LABEL },
    { StatusField,    CPFT_SMALL_TEXT,    const_cast<PWSTR>(L"État"),      GUID_NULL },
    { PasswordField,  CPFT_PASSWORD_TEXT, const_cast<PWSTR>(L"Mot de passe du compte de test"), GUID_NULL },
    { SubmitField,    CPFT_SUBMIT_BUTTON, const_cast<PWSTR>(L"Envoyer"),   GUID_NULL },
};

bool IsValidField(DWORD fieldId)
{
    return fieldId < FieldCount;
}

HRESULT CopyFieldDescriptor(DWORD fieldId, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** copy)
{
    *copy = nullptr;
    if (!IsValidField(fieldId))
    {
        return E_INVALIDARG;
    }

    auto* descriptor = static_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(
        CoTaskMemAlloc(sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR)));
    if (!descriptor)
    {
        return E_OUTOFMEMORY;
    }

    *descriptor = FieldDescriptors[fieldId];
    const HRESULT hr = SHStrDupW(FieldDescriptors[fieldId].pszLabel, &descriptor->pszLabel);
    if (FAILED(hr))
    {
        CoTaskMemFree(descriptor);
        return hr;
    }
    *copy = descriptor;
    return S_OK;
}
