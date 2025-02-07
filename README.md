interface to use for mdb client:

## 接口文档

### 描述
1. create:用于创建一个新的数据集合，并定义数据结构及字段约束。
### 参数说明
- **action**: `string`，必须为 "create"，表示创建操作。
- **name**: `string`，集合的名称。
- **type**: `string`，必须为 "collection"。
- **schema**: `object`，定义集合的字段结构约束,可以不填为空表示无任何约束。
  - **fields**: `object`，包含多个字段定义，每个字段有以下属性：
    - **type**: `string`，字段的数据类型。包括:int, double, bool, string, time, binary, document
    - **constraints**: `object`，字段的约束条件，不填表示无约束。
      - **required**: `boolean`，是否必须提供该字段。
      - **depth**: `int`，表示嵌套深度比特位（1：第一层,2：第二层,3：第一和第二层...每层对应一个比特位）。
      - **minLength**/**maxLength**: `int`，适用于 `string` 类型，定义最小/最大长度。
      - **regexPattern**: `string`，适用于 `string` 类型，正则表达式约束。
      - **minValue**/**maxValue**: `int`，适用于 `int` 类型，定义最小/最大值。

### 示例请求
```
{
    "action": "create",
    "name": "user",
    "type": "collection",
    "schema": {
        "fields": {
            "id": {
                "type": "int",
                "constraints": {
                    "required": true,
                    "depth": 1
                }
            },
            "email": {
                "type": "string",
                "constraints": {
                    "required": false,
                    "minLength": 5,
                    "maxLength": 100,
                    "regexPattern": "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
                }
            },
            "phone": {
                "type": "string",
                "constraints": {
                    "required": false,
                    "minLength": 8,
                    "maxLength": 16,
                    "regexPattern": "^\\+?[1-9]\\d{0,2}[-\\s]?(\\d{3}[-\\s]?){2}\\d{4}$"
                }
            },
            "password": {
                "type": "string",
                "constraints": {
                    "required": false,
                    "minLength": 8,
                    "maxLength": 16,
                    "regexPattern": "^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[#@$!%*?&])[A-Za-z\\d#@$!%*?&]{8,}$"
                }
            },
			"nested": {
                "type": "document",
                "constraints": {
                    "required": true,
                    "depth": 1
                }
            },
            "age": {
                "type": "int",
                "constraints": {
                    "required": false,
                    "minValue": 1,
                    "maxValue": 120
                }
            }
        }
    }
}
```
### 描述
2. insert:用于将数据插入到指定的集合中。
### 参数说明
- **action**: `string`，必须为 "insert"，表示插入操作。
- **name**: `string`，集合的名称。
- **document**: `array`，包含多个文档的数组，每个文档为一个对象，支持嵌套文档。
  - **xxxxxx**: `any`，键值对or嵌入文档
  - **xxxxxx**: `any`，键值对or嵌入文档
  - **xxxxxx**: `any`，键值对or嵌入文档
  ......

### 示例请求
```
{
    "action": "insert",
    "name": "customer_data",
    "type": "collection",
    "documents": [
        {
            "id": 1,
            "title": "whatever",
            "nested": {
                "title": "nested Document 1",
                "value": 12345,
                "details": {
                    "created_at": "${2025-01-01 00:00:00}",
                    "author": "anybody",
                    "age": 25,
                    "email": "xx@yy.zz",
                    "phone": "+10 123 456-1234",
                    "password": "Sb123456#",
                    "stats": {
                        "views": 12345,
                        "likes": 123,
                        "shares": 1234
                    }
                }
            }
        },
        {
            "id": 2,
            "title": "matrix",
			"author": "Neo",
            "version": 1,
            "detail": {
                "power": 999999999,
                "manner": 99999999,
				"armor": 99999999
            }
        }
    ]
}
```

### 描述
3. select:用于从指定的集合中查询数据，支持条件筛选、排序、分页及字段选择。
### 参数说明
- **action**: `string`，必须为 "select"，表示插入操作。
- **name**: `string`，集合的名称。
- **conditions**: `array`，定义查询条件数组,可以不填表示无条件选择所有数据。
  - **path**: `string`，要查询的字段路径，可以使用点表示法来访问嵌套字段（例如 "nested.details.created_at"）
  - **op**: `string`，操作符，用于比较字段值
  - **value**: `any`，用于比较的值(支持null表示不存在)
- **sorting**: `object`，设置查询结果的排序规则，不填表示不排序。
  - **path**: `string`，排序的字段（例如 "nested.details.created_at"）
  - **ascending**: `boolean`，true升序排序，false 表示降序
- **pagination**: `object`，分页信息,不填表示不分页,建议分页返回。
  - **offset**: `int`，查询结果的偏移量，从第几条开始
  - **limit**: `int`，每页返回的结果数
- **fields**: `array`，指定返回的字段列表，可以不填表示选择所有字段。
  - **xxx**: `string`，字段名（例如 "id"）
  - **xxx**: `string`，字段名（例如 "nested.details.created_at"）
  ......

### 示例请求
```
{
    "action": "select",
    "name": "customer_data",
    "conditions": [
        {
            "path": "id",
            "op": "<",
            "value": 100
        },
        {
            "path": "nested.details.created_at",
            "op": ">",
            "value": "${2025-01-01 00:00:00}"
        }
    ],
    "sorting": {
        "path": "nested.details.age",
        "ascending": false
    },
    "pagination": {
        "offset": 0,
        "limit": 10
    },
    "fields": [
        "id",
        "nested.details"
    ]
}
```

### 描述
4. update:用于更新指定集合中的数据，支持条件筛选、字段更新等操作。
### 参数说明
- **action**: `string`，必须为 "update"，表示更新操作。
- **name**: `string`，集合的名称。
- **conditions**: `array`，定义查询条件数组,可以不填表示无条件选择所有数据。
  - **path**: `string`，要查询的字段路径，可以使用点表示法来访问嵌套字段（例如 "nested.details.created_at"）
  - **op**: `string`，操作符，用于比较字段值
  - **value**: `any`，用于比较的值(支持null表示不存在)
- **fields**: `object`，包含要更新的字段及其新值的对象，每个字段包含。
  - **xxx**: `any`，字段名及其值（例如 "id": 1）
  - **xxx**: `any`，字段名及其值（例如 "name": "Neo"）
  ......

### 示例请求
```
{
    "action": "update",
    "name": "customer_data",
    "conditions": [
        {
            "path": "nested.details.age",
            "op": "<",
            "value": 25
        }
    ],
    "fields": {
        "nested.details.author": "nobody",
        "nested.details.age": 30,
        "nested.details.email": "oumss@ou.mass"
    }
}
```

### 描述
5. delete:用于从指定集合中删除符合条件的数据。
### 参数说明
- **action**: `string`，必须为 "delete"，表示删除操作。
- **name**: `string`，集合的名称。
- **conditions**: `array`，定义查询条件数组,可以不填表示无条件选择所有数据。
  - **path**: `string`，要查询的字段路径，可以使用点表示法来访问嵌套字段（例如 "nested.details.created_at"）
  - **op**: `string`，操作符，用于比较字段值
  - **value**: `any`，用于条件判断(支持null表示不存在)
- **fields**: `array`，包含要删除的字段,不填表示删除整个文档。
  - **xxx**: `string`，字段名（例如 "id"）
  - **xxx**: `string`，字段名（例如 "name"）
  ......

### 示例请求
```
{
    "action": "delete",
    "name": "customer_data",
    "conditions": [
        {
            "path": "nested.details.age",
            "op": "==",
            "value": null
        }
    ],
    "fields": [
        "nested.details.age"
    ]
}
```

### 描述
6. create_idx: 用于在指定集合中创建索引，以提高查询效率。
### 参数说明
- **action**: `string`，必须为 "create_idx"，表示创建索引操作。
- **name**: `string`，集合的名称。
- **indexes**: `array`，指定需要创建索引的字段路径列表。每个字段路径用于创建索引，可以使用点表示法访问嵌套字段。
  - **xxx**: `string`，字段名（例如 "id"）
  - **xxx**: `string`，字段名（例如 "name"）
  ......

### 示例请求
```
{
    "action": "create_idx",
    "name": "customer_data",
    "indexes": [
        "id",
        "nested.details.password"
    ]
}
```

### 描述
7. drop_idx: 用于删除指定集合中的索引，以释放存储空间并减少索引维护开销。
### 参数说明
- **action**: `string`，必须为 "drop_idx"，表示删除索引操作。
- **name**: `string`，集合的名称。
- **indexes**: `array`，指定需要删除索引的字段路径列表。每个字段路径用于删除索引，可以使用点表示法访问嵌套字段。
  - **xxx**: `string`，字段名（例如 "id"）
  - **xxx**: `string`，字段名（例如 "name"）
  ......

### 示例请求
```
{
    "action": "drop_idx",
    "name": "customer_data",
    "indexes": [
        "id",
        "nested.details.password"
    ]
}
```