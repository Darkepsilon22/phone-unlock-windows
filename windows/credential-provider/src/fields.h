#pragma once

#include "common.h"

enum FieldId : DWORD
{
    TileImageField,
    LabelField,
    StatusField,
    PasswordField,
    SubmitField,
    FieldCount
};

struct FieldState
{
    CREDENTIAL_PROVIDER_FIELD_STATE visibility;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE interactivity;
};

extern const FieldState FieldStates[FieldCount];
extern const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR FieldDescriptors[FieldCount];

bool IsValidField(DWORD fieldId);
HRESULT CopyFieldDescriptor(DWORD fieldId, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** copy);
