/*
 *  Copyright Beijing 58 Information Technology Co.,Ltd.
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an
 *  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *  KIND, either express or implied.  See the License for the
 *  specific language governing permissions and limitations
 *  under the License.
 */
#include "strHelper.h"
#include "structHelper.h"
#include "serializer.h"
#include "byteHelper.h"
#include <stdio.h>
#include <objc/hash.h>
#include <string.h>
#include <stdlib.h>
cache_ptr structInfoMap;
unsigned int charHashFuncType(void *cachep, const void *key) {
	int hashCode = GetHashcode(key, strlen(key));
	return (hashCode & 0x7FFFFFFF) % ((cache_ptr) cachep)->size;
}

int charCompareFuncType(const void *key1, const void *key2) {
	return strcmp(key1, key2) == 0;
}

unsigned int intHashFuncType(void *cachep, const void *key) {
	int hashCode = *(int*) key;
	return (hashCode & 0x7FFFFFFF) % ((cache_ptr) cachep)->size;
}

int intCompareFuncType(const void *key1, const void *key2) {
	return *(int*) key1 == *(int*) key2;
}
unsigned int objHashFuncType(void *cachep, const void *key) {
	const hashmapEntry *node = key;

	if (node->data == NULL) {
		return 0;
	}
	int hashcode = 0;
	switch (node->typeId) {
	case SERIALIZE_CHAR_N:
		hashcode = *(unsigned char*) node->data % ((cache_ptr) cachep)->size;
		break;
	case SERIALIZE_SHORT_INT_N:
		hashcode = *(unsigned short int*) node->data % ((cache_ptr) cachep)->size;
		break;
	case SERIALIZE_INT_N:
		hashcode = intHashFuncType(cachep, node->data);
		break;
	case SERIALIZE_TIME_N:
		hashcode = *(unsigned long*) node->data % ((cache_ptr) cachep)->size;
		break;
	case SERIALIZE_FLOAT_N: {
		float f = *(float*) node->data;
		hashcode = (unsigned long) f % ((cache_ptr) cachep)->size;
	}
		break;
	case SERIALIZE_DOUBLE_N: {
		double d = *(double*) node->data;
		hashcode = (unsigned long) d % ((cache_ptr) cachep)->size;
	}
		break;
	case SERIALIZE_LONG_LONG_N:
		hashcode = *(unsigned long long*) node->data % ((cache_ptr) cachep)->size;
		break;
	case SERIALIZE_ARRAY_N:
	case SERIALIZE_LIST_N:
	case SERIALIZE_MAP_N:
		hashcode = (unsigned long) node->data % ((cache_ptr) cachep)->size;
		break;
	case SERIALIZE_STRING_N:
		hashcode = charHashFuncType(cachep, node->data);
		break;
	default:
		hashcode = intHashFuncType(cachep, &node->data);
		break;
	}
	return hashcode;
}

int objCompareFuncType(const void *key1, const void *key2) {
	const hashmapEntry *node1 = key1;
	const hashmapEntry *node2 = key2;
	if (node1->typeId != node2->typeId) {
		return 0;
	}

	int typeId = node1->typeId;
	int retFlg = 0;
	switch (typeId) {
	case SERIALIZE_CHAR_N:
		retFlg = *(unsigned char*) node1->data == *(unsigned char*) node2->data;
		break;
	case SERIALIZE_SHORT_INT_N:
		retFlg = *(unsigned short int*) node1->data == *(unsigned short int*) node2->data;
		break;
	case SERIALIZE_INT_N:
		retFlg = intCompareFuncType(node1->data, node2->data);
		break;
	case SERIALIZE_TIME_N:
		retFlg = *(unsigned long*) node1->data == *(unsigned long*) node2->data;
		break;
	case SERIALIZE_FLOAT_N:
		retFlg = *(float*) node1->data == *(float*) node2->data;
		break;
	case SERIALIZE_DOUBLE_N:
		retFlg = *(double*) node1->data == *(double*) node2->data;
		break;
	case SERIALIZE_LONG_LONG_N:
		retFlg = *(unsigned long long*) node1->data == *(unsigned long long*) node2->data;
		break;
	case SERIALIZE_ARRAY_N:
	case SERIALIZE_LIST_N:
	case SERIALIZE_MAP_N:
		retFlg = node1->data == node2->data;
		break;
	case SERIALIZE_STRING_N:
		retFlg = charCompareFuncType(node1->data, node2->data);
		break;
	default:
		retFlg = node1->data == node2->data;
		break;
	}
	return retFlg;
}
char* readNext(char *cursor) {
	while (*cursor != ',' && *cursor != ';' && *cursor != -1) {
		cursor++;
	}
	return cursor;
}
int registerStruct(const char *fileName) {
	structInfoMap = objc_hash_new((unsigned int) 128, intHashFuncType, intCompareFuncType);
	int fileSize;
	char *base;
	char *cursor;
	FILE *fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("Can't open file: %s.", fileName);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	base = malloc(fileSize + 1);
	if (base == NULL) {
		printf("The file %s is too big", fileName);
		fclose(fp);
		exit(0);
	}
	fseek(fp, 0, SEEK_SET);
	fileSize = fread(base, 1, fileSize, fp);
	fclose(fp);

	base[fileSize] = -1;
	cursor = base;
	structFieldInfo *sfi;
	array *value;
	int sfiSize = sizeof(structFieldInfo);
	char *start;
	char c[1024];
	int *key;
	while (*cursor != -1) {
		value = (array*) malloc(sizeof(array));
		value->byteLength = 0;
		value->data = NULL;

		sfi = malloc(sfiSize);
		start = cursor;
		cursor = readNext(cursor);
		sfi->fieldName = malloc(cursor - start + 1);
		memcpy(sfi->fieldName, start, cursor - start);
		sfi->fieldName[cursor - start] = '\0';

		start = ++cursor;
		cursor = readNext(cursor);
		memcpy(c, start, cursor - start);
		c[cursor - start] = '\0';
		sfi->typeId = atoi(c);
		key = malloc(sizeof(int));
		*key = sfi->typeId;

		start = ++cursor;
		cursor = readNext(cursor);
		memcpy(c, start, cursor - start);
		c[cursor - start] = '\0';
		sfi->offset = atoi(c);

		++cursor;
		sfi->isPointe = *cursor & 1;
		++cursor;
		byteArrayPutData(value, sfi, sfiSize);
		++cursor;
		while (*cursor != -1 && *cursor != '\n') {
			sfi = malloc(sfiSize);
			start = cursor;
			cursor = readNext(cursor);
			sfi->fieldName = malloc(cursor - start + 1);
			memcpy(sfi->fieldName, start, cursor - start);
			sfi->fieldName[cursor - start] = '\0';

			start = ++cursor;
			cursor = readNext(cursor);
			memcpy(c, start, cursor - start);
			c[cursor - start] = '\0';
			sfi->typeId = GetTypeId(c);

			start = ++cursor;
			cursor = readNext(cursor);

			start = ++cursor;
			cursor = readNext(cursor);
			memcpy(c, start, cursor - start);
			c[cursor - start] = '\0';
			sfi->offset = atoi(c);

			++cursor;
			sfi->isPointe = *cursor & 1;
			++cursor;

			if (sfi->typeId == SERIALIZE_CHAR_N && sfi->isPointe) {
				sfi->typeId = SERIALIZE_STRING_N;
			}
			if (sfi->typeId == SERIALIZE_MAP_N) {
				sfi->isPointe = 1;
			}

			byteArrayPutData(value, sfi, sfiSize);
			++cursor;
		}
		objc_hash_add(&structInfoMap, key, value);
		++cursor;
	}
	free(base);
	return 0;
}
