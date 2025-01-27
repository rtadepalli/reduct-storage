---
description: HTTP methods to read, write and query entry records
---

# Entry API

The Entry API allows users to write and read data from their buckets, as well as search for specific entries using query operations.



{% swagger method="post" path=" " baseUrl="/api/v1/b/:bucket_name/:entry_name" summary="Write a record to an entry" %}
{% swagger-description %}
The storage engine creates an entry on the first write operation. The record should be placed in the body of the HTTP request. The body can also be empty.

The method needs a valid API token with write access to the entry's bucket if authentication is enabled.
{% endswagger-description %}

{% swagger-parameter in="path" name=":bucket_name" required="true" %}
Name of bucket
{% endswagger-parameter %}

{% swagger-parameter in="path" name=":entry_name" required="true" %}
Name of entry
{% endswagger-parameter %}

{% swagger-parameter in="query" name="ts" type="Integer" required="true" %}
A UNIX timestamp in microseconds
{% endswagger-parameter %}

{% swagger-parameter in="header" required="true" name="Content-Length" %}
Content-length is required to start an asynchronous write operation
{% endswagger-parameter %}

{% swagger-response status="200: OK" description="The record is written" %}
```javascript
{
    // Response
}
```
{% endswagger-response %}

{% swagger-response status="400: Bad Request" description="Posted content bigger or smaller than content-length" %}
```javascript
{
    // Response
}
```
{% endswagger-response %}

{% swagger-response status="401: Unauthorized" description="Access token is invalid or empty" %}
```javascript
{
    "detail": "error_message"
}
```
{% endswagger-response %}

{% swagger-response status="403: Forbidden" description="Access token does not have write permissions" %}
```javascript
{
    "detail": "error_message"
}
```
{% endswagger-response %}

{% swagger-response status="404: Not Found" description="Bucket is not found" %}
```javascript
{
    "detail": "string"
}
```
{% endswagger-response %}

{% swagger-response status="409: Conflict" description="A record with the same timestamp already exists" %}
```javascript
{
   "detail": "string"
}
```
{% endswagger-response %}

{% swagger-response status="422: Unprocessable Entity" description="Bad timestamp" %}
```javascript
{
   "detail": "string"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger method="get" path=" " baseUrl="/api/v1/b/:bucket_name/:entry_name " summary="Get a record from an entry" %}
{% swagger-description %}
The method finds a record for the given timestamp and sends its content in the HTTP response body. It also sends additional information in headers:

**x-reduct-time** - UNIX timestamp of the record in microseconds

**x-reduct-last** - 1 - if a record is the last record in the query

If authentication is enabled, the method needs a valid API token with read access to the entry's bucket.
{% endswagger-description %}

{% swagger-parameter in="path" name=":bucket_name" required="true" %}
Name of bucket
{% endswagger-parameter %}

{% swagger-parameter in="query" name="q" type="Integer" required="false" %}
A query ID to read the next record in the query. If it is set, the parameter

`ts`

is ignored.
{% endswagger-parameter %}

{% swagger-parameter in="path" name=":entry_name" required="true" %}
Name of entry
{% endswagger-parameter %}

{% swagger-parameter in="query" name="ts" type="Integer" required="false" %}
A UNIX timestamp in microseconds. If it is empty, the latest record is returned.
{% endswagger-parameter %}

{% swagger-response status="200: OK" description="The record is found and returned in body of the response" %}
```javascript
"string"
```
{% endswagger-response %}

{% swagger-response status="401: Unauthorized" description="Access token is invalid or empty" %}
```javascript
{
    "detail": "error_message"
}
```
{% endswagger-response %}

{% swagger-response status="403: Forbidden" description="Access token doesn" %}
```javascript
{
    "detail": "error_message"
}
```
{% endswagger-response %}

{% swagger-response status="404: Not Found" description="The bucket or record with the timestamp doesn" %}
```javascript
{
   "detail": "string"
}
```
{% endswagger-response %}

{% swagger-response status="422: Unprocessable Entity" description="Bad timestamp" %}
```javascript
{
   "detail": "string"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger method="get" path="" baseUrl="/api/v1/b/:bucket_name/:entry_name/q " summary="Query records for a time interval" %}
{% swagger-description %}
The method responds with a JSON document containing an ID which should be used to read records with the following endpoint:

**GET /b/:bucket\_name/:entry\_name?q=ID.**

The time interval is \[start, stop).

If authentication is enabled, the method needs a valid API token with read access to the bucket of the entry.
{% endswagger-description %}

{% swagger-parameter in="path" name=":bucket_name" required="true" %}
Name of bucket
{% endswagger-parameter %}

{% swagger-parameter in="query" name="start" type="Integer" required="false" %}
A UNIX timestamp in microseconds. If not set, the query starts from the oldest record in the entry.
{% endswagger-parameter %}

{% swagger-parameter in="query" name="stop" type="Integer" required="false" %}
A UNIX timestamp in microseconds. If not set, the query starts from the latest record in the entry.
{% endswagger-parameter %}

{% swagger-parameter in="path" name=":entry_name" required="true" %}
Name of entry
{% endswagger-parameter %}

{% swagger-parameter in="query" name="ttl" type="Integer" required="false" %}
Time To Live of the query in seconds. If a client haven't read any record for this time interval, the server removes the query and the query ID becomes invalid. Default value 5 seconds.
{% endswagger-parameter %}

{% swagger-response status="200: OK" description="" %}
```javascript
{
   "id": "integer" // ID of query wich can be used in GET /b/:bucket/:entry request
}
```
{% endswagger-response %}

{% swagger-response status="204: No Content" description="No records for the time interval" %}
```javascript
{
    // Response
}
```
{% endswagger-response %}

{% swagger-response status="401: Unauthorized" description="Access token is invalid or empty" %}
```javascript
{
    // Response
}
```
{% endswagger-response %}

{% swagger-response status="403: Forbidden" description="Access token doesn" %}
```javascript
{
    // Response
}
```
{% endswagger-response %}

{% swagger-response status="404: Not Found" description="The bucket doesn" %}
```javascript
{
   "detail": "string"
}
```
{% endswagger-response %}

{% swagger-response status="422: Unprocessable Entity" description="One or both timestamps are bad , or TTL is not a number" %}
```javascript
{
   "detail": "string"
}
```
{% endswagger-response %}
{% endswagger %}
